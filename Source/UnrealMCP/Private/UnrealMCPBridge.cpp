/**
 * @file UnrealMCPBridge.cpp
 * @brief Unreal MCP 桥接子系统实现，负责 TCP 服务与命令路由。
 */
#include "UnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Commands/UnrealMCPUMGCommands.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

class FUnrealMCPBridgeSocketDestroyer
{
public:
    void operator()(FSocket* Socket) const
    {
        if (!Socket)
        {
            return;
        }

        if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
        {
            SocketSubsystem->DestroySocket(Socket);
            return;
        }

        delete Socket;
    }
};

static TSharedPtr<FSocket> MakeUnrealMCPBridgeSocketShareable(FSocket* Socket)
{
    if (!Socket)
    {
        return TSharedPtr<FSocket>();
    }

    return MakeShareable(Socket, FUnrealMCPBridgeSocketDestroyer());
}

/**
 * @brief 构造函数，初始化各类命令处理器。
 */
UUnrealMCPBridge::UUnrealMCPBridge()
{
    EditorCommands = MakeShared<FUnrealMCPEditorCommands>();
    AssetCommands = MakeShared<FUnrealMCPAssetCommands>();
    BlueprintCommands = MakeShared<FUnrealMCPBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FUnrealMCPBlueprintNodeCommands>();
    ProjectCommands = MakeShared<FUnrealMCPProjectCommands>();
    UMGCommands = MakeShared<FUnrealMCPUMGCommands>();
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerRunnable = nullptr;
    ServerThread = nullptr;
}

/**
 * @brief 析构函数，释放命令处理器。
 */
UUnrealMCPBridge::~UUnrealMCPBridge()
{
    EditorCommands.Reset();
    AssetCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    ProjectCommands.Reset();
    UMGCommands.Reset();
}

/**
 * @brief 编辑器子系统初始化。
 * @param [in] Collection 子系统集合。
 */
void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerRunnable = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    if (IsRunningCommandlet())
    {
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Running in commandlet mode, skip MCP server startup"));
        return;
    }

    // Start the server automatically
    StartServer();
}

/**
 * @brief 编辑器子系统反初始化。
 */
void UUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
    StopServer();
}

/**
 * @brief 启动 TCP 监听服务并创建服务线程。
 * @note 若监听套接字或线程创建失败，会提前返回并记录错误日志。
 */
void UUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeUnrealMCPBridgeSocketShareable(
        SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerRunnable = new FMCPServerRunnable(this, ListenerSocket);
    ServerThread = FRunnableThread::Create(
        ServerRunnable,
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));
        delete ServerRunnable;
        ServerRunnable = nullptr;
        StopServer();
        return;
    }
}

/**
 * @brief 停止 TCP 服务并释放线程与套接字资源。
 */
void UUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    if (ServerRunnable)
    {
        ServerRunnable->Stop();
    }

    if (ConnectionSocket.IsValid())
    {
        ConnectionSocket->Close();
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ListenerSocket->Close();
        ListenerSocket.Reset();
    }

    if (ServerThread)
    {
        ServerThread->WaitForCompletion();
        delete ServerThread;
        ServerThread = nullptr;
    }

    if (ServerRunnable)
    {
        delete ServerRunnable;
        ServerRunnable = nullptr;
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

/**
 * @brief 执行来自客户端的命令并返回 JSON 字符串。
 * @param [in] CommandType 命令类型。
 * @param [in] Params 命令参数。
 * @return FString 序列化后的 JSON 响应。
 * @note 实际命令处理在游戏线程中执行，以避免跨线程访问编辑器对象。
 */
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            // Editor Commands (including actor manipulation)
            else if (CommandType == TEXT("get_actors_in_level") || 
                     CommandType == TEXT("make_directory") ||
                     CommandType == TEXT("duplicate_asset") ||
                     CommandType == TEXT("load_level") ||
                     CommandType == TEXT("save_current_level") ||
                     CommandType == TEXT("start_pie") ||
                     CommandType == TEXT("start_vr_preview") ||
                     CommandType == TEXT("start_standalone_game") ||
                     CommandType == TEXT("stop_pie") ||
                     CommandType == TEXT("get_play_state") ||
                     CommandType == TEXT("start_live_coding") ||
                     CommandType == TEXT("compile_live_coding") ||
                     CommandType == TEXT("get_live_coding_state") ||
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("find_actors") ||
                     CommandType == TEXT("spawn_actor") ||
                     CommandType == TEXT("spawn_actor_from_class") ||
                     CommandType == TEXT("create_actor") ||
                     CommandType == TEXT("delete_actor") || 
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("get_actor_properties") ||
                     CommandType == TEXT("get_actor_components") ||
                     CommandType == TEXT("get_scene_components") ||
                     CommandType == TEXT("get_world_settings") ||
                     CommandType == TEXT("set_world_settings") ||
                     CommandType == TEXT("get_data_layers") ||
                     CommandType == TEXT("create_data_layer") ||
                     CommandType == TEXT("set_actor_data_layers") ||
                     CommandType == TEXT("set_data_layer_state") ||
                     CommandType == TEXT("set_actor_property") ||
                     CommandType == TEXT("set_actor_tags") ||
                     CommandType == TEXT("set_actor_folder_path") ||
                     CommandType == TEXT("set_actor_visibility") ||
                     CommandType == TEXT("set_actor_mobility") ||
                     CommandType == TEXT("spawn_blueprint_actor") ||
                     CommandType == TEXT("duplicate_actor") ||
                     CommandType == TEXT("select_actor") ||
                     CommandType == TEXT("get_selected_actors") ||
                     CommandType == TEXT("get_editor_selection") ||
                     CommandType == TEXT("create_light") ||
                     CommandType == TEXT("set_light_properties") ||
                     CommandType == TEXT("capture_scene_to_render_target") ||
                     CommandType == TEXT("set_post_process_settings") ||
                     CommandType == TEXT("attach_actor") ||
                     CommandType == TEXT("detach_actor") ||
                     CommandType == TEXT("add_component_to_actor") ||
                     CommandType == TEXT("remove_component_from_actor") ||
                     CommandType == TEXT("set_actors_transform") ||
                     CommandType == TEXT("focus_viewport") || 
                     CommandType == TEXT("take_screenshot") ||
                     CommandType == TEXT("take_highres_screenshot") ||
                     CommandType == TEXT("capture_viewport_sequence") ||
                     CommandType == TEXT("open_asset_editor") ||
                     CommandType == TEXT("close_asset_editor") ||
                     CommandType == TEXT("execute_console_command") ||
                     CommandType == TEXT("execute_unreal_python") ||
                     CommandType == TEXT("run_editor_utility_widget") ||
                     CommandType == TEXT("run_editor_utility_blueprint") ||
                     CommandType == TEXT("set_viewport_mode") ||
                     CommandType == TEXT("get_viewport_camera") ||
                     CommandType == TEXT("get_output_log") ||
                     CommandType == TEXT("clear_output_log") ||
                     CommandType == TEXT("get_message_log") ||
                     CommandType == TEXT("line_trace") ||
                     CommandType == TEXT("box_trace") ||
                     CommandType == TEXT("sphere_trace") ||
                     CommandType == TEXT("get_hit_result_under_cursor") ||
                     CommandType == TEXT("show_editor_notification"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Asset Commands
                else if (CommandType == TEXT("search_assets") ||
                     CommandType == TEXT("get_asset_metadata") ||
                     CommandType == TEXT("get_asset_dependencies") ||
                     CommandType == TEXT("get_asset_referencers") ||
                     CommandType == TEXT("create_asset") ||
                     CommandType == TEXT("create_data_asset") ||
                     CommandType == TEXT("create_primary_data_asset") ||
                     CommandType == TEXT("create_curve") ||
                     CommandType == TEXT("create_data_table") ||
                     CommandType == TEXT("get_data_table_rows") ||
                     CommandType == TEXT("import_data_table") ||
                     CommandType == TEXT("set_data_table_row") ||
                     CommandType == TEXT("export_data_table") ||
                     CommandType == TEXT("remove_data_table_row") ||
                     CommandType == TEXT("save_asset") ||
                     CommandType == TEXT("import_asset") ||
                     CommandType == TEXT("export_asset") ||
                     CommandType == TEXT("reimport_asset") ||
                     CommandType == TEXT("fixup_redirectors") ||
                     CommandType == TEXT("get_asset_summary") ||
                     CommandType == TEXT("get_blueprint_summary") ||
                     CommandType == TEXT("rename_asset") ||
                     CommandType == TEXT("move_asset") ||
                     CommandType == TEXT("delete_asset") ||
                     CommandType == TEXT("batch_rename_assets") ||
                     CommandType == TEXT("batch_move_assets") ||
                     CommandType == TEXT("set_asset_metadata") ||
                     CommandType == TEXT("consolidate_assets") ||
                     CommandType == TEXT("replace_asset_references") ||
                     CommandType == TEXT("get_selected_assets") ||
                     CommandType == TEXT("sync_content_browser_to_assets") ||
                     CommandType == TEXT("save_all_dirty_assets") ||
                     CommandType == TEXT("create_material") ||
                     CommandType == TEXT("create_material_function") ||
                     CommandType == TEXT("create_render_target") ||
                     CommandType == TEXT("create_material_instance") ||
                     CommandType == TEXT("get_material_parameters") ||
                     CommandType == TEXT("set_material_instance_scalar_parameter") ||
                     CommandType == TEXT("set_material_instance_vector_parameter") ||
                     CommandType == TEXT("set_material_instance_texture_parameter") ||
                     CommandType == TEXT("assign_material_to_actor") ||
                     CommandType == TEXT("assign_material_to_component") ||
                     CommandType == TEXT("replace_material_slot") ||
                     CommandType == TEXT("add_material_expression") ||
                     CommandType == TEXT("connect_material_expressions") ||
                     CommandType == TEXT("layout_material_graph") ||
                     CommandType == TEXT("compile_material"))
                {
                ResultJson = AssetCommands->HandleCommand(CommandType, Params);
                }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") || 
                     CommandType == TEXT("create_child_blueprint") ||
                     CommandType == TEXT("add_component_to_blueprint") || 
                     CommandType == TEXT("remove_component_from_blueprint") ||
                     CommandType == TEXT("attach_component_in_blueprint") ||
                     CommandType == TEXT("set_component_property") || 
                     CommandType == TEXT("set_physics_properties") || 
                     CommandType == TEXT("compile_blueprint") || 
                     CommandType == TEXT("compile_blueprints") ||
                     CommandType == TEXT("cleanup_blueprint_for_reparent") ||
                     CommandType == TEXT("set_blueprint_property") || 
                     CommandType == TEXT("set_game_mode_default_pawn") ||
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("set_pawn_properties") ||
                     CommandType == TEXT("add_blueprint_variable") ||
                     CommandType == TEXT("delete_blueprint_variable") ||
                     CommandType == TEXT("remove_unused_blueprint_variables") ||
                     CommandType == TEXT("add_blueprint_interface") ||
                     CommandType == TEXT("set_blueprint_variable_default") ||
                     CommandType == TEXT("add_blueprint_function") ||
                     CommandType == TEXT("delete_blueprint_function") ||
                     CommandType == TEXT("get_blueprint_compile_errors") ||
                     CommandType == TEXT("rename_blueprint_member") ||
                     CommandType == TEXT("save_blueprint") ||
                     CommandType == TEXT("open_blueprint_editor") ||
                     CommandType == TEXT("reparent_blueprint"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Node Commands
            else if (CommandType == TEXT("connect_blueprint_nodes") || 
                     CommandType == TEXT("disconnect_blueprint_nodes") ||
                     CommandType == TEXT("move_blueprint_node") ||
                     CommandType == TEXT("layout_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_comment_node") ||
                     CommandType == TEXT("add_blueprint_reroute_node") ||
                     CommandType == TEXT("create_blueprint_graph") ||
                     CommandType == TEXT("delete_blueprint_graph") ||
                     CommandType == TEXT("add_blueprint_get_self_component_reference") ||
                     CommandType == TEXT("add_blueprint_self_reference") ||
                     CommandType == TEXT("spawn_blueprint_node") ||
                     CommandType == TEXT("describe_blueprint_node") ||
                     CommandType == TEXT("set_blueprint_pin_default") ||
                     CommandType == TEXT("delete_blueprint_node") ||
                     CommandType == TEXT("duplicate_blueprint_subgraph") ||
                     CommandType == TEXT("collapse_nodes_to_function") ||
                     CommandType == TEXT("collapse_nodes_to_macro") ||
                     CommandType == TEXT("validate_blueprint_graph") ||
                     CommandType == TEXT("find_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_event_node") ||
                     CommandType == TEXT("add_blueprint_input_action_node") ||
                     CommandType == TEXT("add_blueprint_function_node") ||
                     CommandType == TEXT("add_blueprint_get_component_node"))
            {
                ResultJson = BlueprintNodeCommands->HandleCommand(CommandType, Params);
            }
            // Project Commands
            else if (CommandType == TEXT("create_input_mapping") ||
                     CommandType == TEXT("create_input_axis_mapping") ||
                     CommandType == TEXT("list_input_mappings") ||
                     CommandType == TEXT("remove_input_mapping") ||
                     CommandType == TEXT("create_input_action_asset") ||
                     CommandType == TEXT("create_input_mapping_context") ||
                     CommandType == TEXT("add_mapping_to_context") ||
                     CommandType == TEXT("assign_mapping_context") ||
                     CommandType == TEXT("get_project_setting") ||
                     CommandType == TEXT("set_project_setting") ||
                     CommandType == TEXT("set_default_maps") ||
                     CommandType == TEXT("set_game_framework_defaults"))
            {
                ResultJson = ProjectCommands->HandleCommand(CommandType, Params);
            }
            // UMG Commands
            else if (CommandType == TEXT("create_umg_widget_blueprint") ||
                     CommandType == TEXT("add_text_block_to_widget") ||
                     CommandType == TEXT("add_button_to_widget") ||
                     CommandType == TEXT("bind_widget_event") ||
                     CommandType == TEXT("set_text_block_binding") ||
                     CommandType == TEXT("bind_widget_property") ||
                     CommandType == TEXT("add_widget_to_viewport") ||
                     CommandType == TEXT("remove_widget_from_viewport") ||
                     CommandType == TEXT("add_image_to_widget") ||
                     CommandType == TEXT("add_border_to_widget") ||
                     CommandType == TEXT("add_canvas_panel_to_widget") ||
                     CommandType == TEXT("add_horizontal_box_to_widget") ||
                     CommandType == TEXT("add_vertical_box_to_widget") ||
                     CommandType == TEXT("add_overlay_to_widget") ||
                     CommandType == TEXT("add_scroll_box_to_widget") ||
                     CommandType == TEXT("add_size_box_to_widget") ||
                     CommandType == TEXT("add_spacer_to_widget") ||
                     CommandType == TEXT("add_progress_bar_to_widget") ||
                     CommandType == TEXT("add_slider_to_widget") ||
                     CommandType == TEXT("add_check_box_to_widget") ||
                     CommandType == TEXT("add_editable_text_to_widget") ||
                     CommandType == TEXT("add_rich_text_to_widget") ||
                     CommandType == TEXT("add_multi_line_text_to_widget") ||
                     CommandType == TEXT("add_named_slot_to_widget") ||
                     CommandType == TEXT("add_list_view_to_widget") ||
                     CommandType == TEXT("add_tile_view_to_widget") ||
                     CommandType == TEXT("add_tree_view_to_widget") ||
                     CommandType == TEXT("remove_widget_from_blueprint") ||
                     CommandType == TEXT("set_widget_slot_layout") ||
                     CommandType == TEXT("set_widget_visibility") ||
                     CommandType == TEXT("set_widget_style") ||
                     CommandType == TEXT("set_widget_brush") ||
                     CommandType == TEXT("create_widget_animation") ||
                     CommandType == TEXT("add_widget_animation_keyframe") ||
                     CommandType == TEXT("open_widget_blueprint_editor"))
            {
                ResultJson = UMGCommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("未知命令: %s"), *CommandType));
                
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }

            if (!ResultJson.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: 命令 %s 没有返回有效 JSON 结果"), *CommandType);
                ResultJson = FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("命令 %s 没有返回有效结果"), *CommandType));
            }
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;
            
            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess && ResultJson->HasField(TEXT("error")))
                {
                    ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                }
            }
            
            if (bSuccess)
            {
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("执行命令时发生异常: %s"), UTF8_TO_TCHAR(e.what())));
        }
        catch (...)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), TEXT("执行命令时发生未知异常"));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    return Future.Get();
}
