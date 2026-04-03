"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import socket
import sys
import json
import time
import threading
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp.log', encoding='utf-8'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("UnrealMCP")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557
DEFAULT_SOCKET_TIMEOUT_SECONDS = 10.0
SLOW_COMMAND_TIMEOUT_SECONDS = 30.0
LIVE_CODING_WAIT_TIMEOUT_SECONDS = 120.0
LIVE_CODING_STATE_FALLBACK_TIMEOUT_SECONDS = 5.0
LIVE_CODING_STATE_FALLBACK_POLL_SECONDS = 15.0
LIVE_CODING_STATE_FALLBACK_POLL_INTERVAL_SECONDS = 0.5
BLUEPRINT_COMMAND_TIMEOUT_SECONDS = 20.0
SOCKET_CONNECT_RETRY_COUNT = 3
SOCKET_CONNECT_RETRY_DELAY_SECONDS = 0.2

SLOW_RESPONSE_COMMANDS = {
    "load_level",
    "save_current_level",
    "save_all_dirty_assets",
    "import_asset",
    "export_asset",
    "reimport_asset",
    "fixup_redirectors",
    "get_asset_summary",
    "get_blueprint_summary",
    "create_asset",
    "take_highres_screenshot",
    "capture_viewport_sequence",
    "open_asset_editor",
    "run_editor_utility_widget",
    "run_editor_utility_blueprint",
    "start_vr_preview",
    "start_standalone_game",
    "search_assets",
    "get_asset_metadata",
    "get_asset_dependencies",
    "get_asset_referencers",
}

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
        self._command_lock = threading.RLock()
        self._command_sequence = 0

    def _close_socket(self):
        """Close the current socket and reset connection state."""
        if self.socket:
            try:
                self.socket.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass

            try:
                self.socket.close()
            except Exception:
                pass

        self.socket = None
        self.connected = False

    def _should_retry_connect(self, error: Exception) -> bool:
        """Return whether the current connect failure is worth retrying."""
        if isinstance(error, socket.timeout):
            return True

        if isinstance(error, OSError):
            error_number = getattr(error, "errno", None)
            if error_number in {10054, 10060, 10061, 104, 110, 111}:
                return True

        return False

    def _next_command_id(self) -> int:
        """Allocate a monotonically increasing command id for logging."""
        self._command_sequence += 1
        return self._command_sequence
    
    def connect(
        self,
        timeout_seconds: float = DEFAULT_SOCKET_TIMEOUT_SECONDS,
        command_name: str = "",
        command_id: int = 0
    ) -> bool:
        """Connect to the Unreal Engine instance."""
        command_label = f"命令#{command_id} {command_name}" if command_name else "命令"

        for attempt_index in range(1, SOCKET_CONNECT_RETRY_COUNT + 1):
            try:
                self._close_socket()

                logger.info(
                    "%s 正在连接 Unreal %s:%s，第 %d/%d 次",
                    command_label,
                    UNREAL_HOST,
                    UNREAL_PORT,
                    attempt_index,
                    SOCKET_CONNECT_RETRY_COUNT,
                )
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.settimeout(timeout_seconds)
                
                # Set socket options for better stability
                self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                
                # Set larger buffer sizes
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
                
                self.socket.connect((UNREAL_HOST, UNREAL_PORT))
                self.connected = True
                logger.info("%s 已连接到 Unreal Engine", command_label)
                return True
                
            except Exception as error:
                self._close_socket()
                retryable = attempt_index < SOCKET_CONNECT_RETRY_COUNT and self._should_retry_connect(error)
                logger.warning(
                    "%s 连接 Unreal 失败，第 %d/%d 次，错误=%s，%s",
                    command_label,
                    attempt_index,
                    SOCKET_CONNECT_RETRY_COUNT,
                    error,
                    "准备重试" if retryable else "不再重试",
                )
                if retryable:
                    time.sleep(SOCKET_CONNECT_RETRY_DELAY_SECONDS)
                    continue

                return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        with self._command_lock:
            self._close_socket()

    def receive_full_response(
        self,
        sock,
        buffer_size=4096,
        timeout_seconds: float = DEFAULT_SOCKET_TIMEOUT_SECONDS,
        command_label: str = ""
    ) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(timeout_seconds)
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                data = b''.join(chunks)
                try:
                    decoded_data = data.decode('utf-8')
                except UnicodeDecodeError:
                    logger.debug("Received partial UTF-8 response chunk, waiting for more data...")
                    continue

                try:
                    json.loads(decoded_data)
                    logger.info("%s 收到完整响应，字节数=%d", command_label, len(data))
                    return data
                except json.JSONDecodeError:
                    logger.debug("%s 收到分段响应，继续等待剩余数据", command_label)
                    continue
                except Exception as e:
                    logger.warning("%s 处理响应分段时出错: %s", command_label, e)
                    continue
        except socket.timeout:
            logger.warning("%s 在 %.1f 秒后读取响应超时", command_label, timeout_seconds)
            if chunks:
                data = b''.join(chunks)
                try:
                    decoded_data = data.decode('utf-8')
                    json.loads(decoded_data)
                    logger.info("%s 超时后采用已完整拼出的响应，字节数=%d", command_label, len(data))
                    return data
                except UnicodeDecodeError:
                    logger.debug("%s 缓冲响应以不完整 UTF-8 结尾", command_label)
                except Exception:
                    pass
            raise Exception(f"{command_label} 在 {timeout_seconds:.1f} 秒内未收到 Unreal 响应")
        except Exception as e:
            logger.error("%s 读取响应时出错: %s", command_label, e)
            raise

    def _get_response_timeout_seconds(self, command: str, params: Dict[str, Any]) -> float:
        """Resolve a command-specific response timeout."""
        if command == "compile_live_coding" and params.get("wait_for_completion"):
            return LIVE_CODING_WAIT_TIMEOUT_SECONDS
        if command in SLOW_RESPONSE_COMMANDS:
            return SLOW_COMMAND_TIMEOUT_SECONDS
        if command in {
            "find_blueprint_nodes",
            "describe_blueprint_node",
            "spawn_blueprint_node",
            "set_blueprint_pin_default",
            "delete_blueprint_node",
            "connect_blueprint_nodes",
            "compile_blueprint"
        }:
            return BLUEPRINT_COMMAND_TIMEOUT_SECONDS
        return DEFAULT_SOCKET_TIMEOUT_SECONDS

    def _query_live_coding_state_after_timeout(self) -> Optional[Dict[str, Any]]:
        """Best-effort state query used when compile_live_coding timed out."""
        deadline = time.monotonic() + LIVE_CODING_STATE_FALLBACK_POLL_SECONDS
        last_response: Optional[Dict[str, Any]] = None

        while time.monotonic() < deadline:
            state_response = self.send_command(
                "get_live_coding_state",
                {},
                response_timeout_seconds=LIVE_CODING_STATE_FALLBACK_TIMEOUT_SECONDS,
                allow_live_coding_timeout_fallback=False
            )
            if not state_response:
                time.sleep(LIVE_CODING_STATE_FALLBACK_POLL_INTERVAL_SECONDS)
                continue

            last_response = state_response
            if state_response.get("status") == "success":
                result = state_response.get("result", {})
                if not result.get("is_compiling", False):
                    return state_response

            time.sleep(LIVE_CODING_STATE_FALLBACK_POLL_INTERVAL_SECONDS)

        return last_response
    
    def send_command(
        self,
        command: str,
        params: Dict[str, Any] = None,
        response_timeout_seconds: Optional[float] = None,
        allow_live_coding_timeout_fallback: bool = True
    ) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        with self._command_lock:
            # Unreal 侧当前仍以“短连接单命令”模型最稳，避免复用陈旧 socket。
            self._close_socket()

            command_params = params or {}
            timeout_seconds = response_timeout_seconds
            if timeout_seconds is None:
                timeout_seconds = self._get_response_timeout_seconds(command, command_params)

            command_id = self._next_command_id()
            command_label = f"命令#{command_id} {command}"

            if not self.connect(
                timeout_seconds=DEFAULT_SOCKET_TIMEOUT_SECONDS,
                command_name=command,
                command_id=command_id
            ):
                logger.error("%s 连接 Unreal Engine 失败", command_label)
                return {
                    "status": "error",
                    "error": f"{command_label} 连接 Unreal Engine 失败"
                }
            
            try:
                command_obj = {
                    "type": command,
                    "params": command_params
                }
                
                command_json = json.dumps(command_obj)
                logger.info("%s 开始发送，参数字段=%s", command_label, list(command_params.keys()))
                self.socket.sendall(command_json.encode('utf-8'))
                
                response_data = self.receive_full_response(
                    self.socket,
                    timeout_seconds=timeout_seconds,
                    command_label=command_label
                )
                response = json.loads(response_data.decode('utf-8'))
                logger.info("%s 收到响应，status=%s", command_label, response.get("status"))
                
                # Check for both error formats: {"status": "error", ...} and {"success": false, ...}
                if response.get("status") == "error":
                    error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                    logger.error("%s 返回错误: %s", command_label, error_message)
                    if "error" not in response:
                        response["error"] = error_message
                elif response.get("success") is False:
                    error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                    logger.error("%s 返回 success=false: %s", command_label, error_message)
                    response = {
                        "status": "error",
                        "error": error_message
                    }
                
                return response
                
            except Exception as e:
                logger.error("%s 发送命令失败: %s", command_label, e)
                error_message = str(e)

                if (allow_live_coding_timeout_fallback and
                    command == "compile_live_coding" and
                    command_params.get("wait_for_completion") and
                    "未收到 Unreal 响应" in error_message):
                    live_coding_state = self._query_live_coding_state_after_timeout()
                    response: Dict[str, Any] = {
                        "status": "error",
                        "error": error_message,
                        "timed_out_waiting_for_response": True
                    }
                    if live_coding_state is not None:
                        response["live_coding_state"] = live_coding_state.get("result", live_coding_state)
                    return response

                return {
                    "status": "error",
                    "error": error_message
                }
            finally:
                self._close_socket()

# Global connection state
_unreal_connection: UnrealConnection = None
_unreal_connection_lock = threading.Lock()

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        with _unreal_connection_lock:
            if _unreal_connection is None:
                _unreal_connection = UnrealConnection()
            return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("UnrealMCP server starting up")
    try:
        _unreal_connection = UnrealConnection()
        logger.info("已初始化 UnrealConnection，等待工具按需建立 socket 连接")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        _unreal_connection = None
    
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP server shut down")

# Initialize server
mcp = FastMCP(
    "UnrealMCP",
    description="Unreal Engine integration via Model Context Protocol",
    lifespan=server_lifespan
)

# Import and register tools
from tools.editor_tools import register_editor_tools
from tools.asset_tools import register_asset_tools
from tools.blueprint_tools import register_blueprint_tools
from tools.node_tools import register_blueprint_node_tools
from tools.project_tools import register_project_tools
from tools.umg_tools import register_umg_tools
from tools.meta_tools import register_meta_tools

# Register tools
register_editor_tools(mcp)
register_asset_tools(mcp)
register_blueprint_tools(mcp)
register_blueprint_node_tools(mcp)
register_project_tools(mcp)
register_umg_tools(mcp)
register_meta_tools(mcp)

@mcp.prompt()
def info():
    """Information about available Unreal MCP tools and best practices."""
    return """
    # Unreal MCP Server Tools and Best Practices
    
    ## UMG (Widget Blueprint) Tools
    - `create_umg_widget_blueprint(widget_name, parent_class="UserWidget", path="/Game/UI")` 
      Create a new UMG Widget Blueprint
    - `add_text_block_to_widget(widget_name, text_block_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1])`
      Add a Text Block widget with customizable properties
    - `add_button_to_widget(widget_name, button_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1], background_color=[0.1,0.1,0.1,1])`
      Add a Button widget with text and styling
    - `bind_widget_event(widget_name, widget_component_name, event_name, function_name="")`
      Bind events like OnClicked to functions
    - `add_widget_to_viewport(widget_name, z_order=0)`
      Add widget instance to game viewport during PIE
    - `set_text_block_binding(widget_name, text_block_name, binding_property, binding_type="Text")`
      Set up dynamic property binding for text blocks, binding_type supports Text/Visibility/ColorAndOpacity/ShadowColorAndOpacity/ToolTipText/IsEnabled

    ## Editor Tools
    ### Viewport and Screenshots
    - `focus_viewport(target=None, location=None, distance=1000.0, orientation=None)` - Focus the active editor viewport on an actor or location
    - `take_screenshot(filepath)` - Capture a screenshot of the active viewport and save it to `filepath`
    - `start_pie(simulate=False)` - Start Play-In-Editor session
    - `start_vr_preview()` - Start a VR Preview session
    - `stop_pie()` - Stop current Play-In-Editor session
    - `get_play_state()` - Query current play state, including `is_playing`, `is_play_session_queued`, `is_vr_preview` and `play_mode`
    - `make_directory(directory_path)` - Create a Content Browser directory, path must start with `/Game`
    - `duplicate_asset(source_asset_path, destination_asset_path, overwrite=False)` - Duplicate an asset to another content path
    - `load_level(level_path)` - Load a level by content path
    - `save_current_level()` - Save the current editor level
    - `start_live_coding(show_console=True)` - Enable Live Coding for current editor session
    - `compile_live_coding(wait_for_completion=False, show_console=True)` - Trigger Live Coding compile
    - `get_live_coding_state()` - Query current Live Coding session state

    ### Actor Management
    - `get_actors_in_level(include_components=False, detailed_components=True, world_type="auto")` - Return actor query result object for selected world (auto/editor/pie)
    - `find_actors_by_name(pattern, world_type="auto", include_components=False, detailed_components=True)` - Return actor query result object filtered by name pattern
    - `get_scene_components(pattern="", detailed_components=True, world_type="auto")` - Query all scene actors and their components
    - `get_actor_components(name="", actor_path="", detailed_components=True, world_type="auto")` - Query all components for one actor
    - `spawn_actor(name, type, location=[0,0,0], rotation=[0,0,0], scale=[1,1,1])` - Create actors
    - `delete_actor(name)` - Remove actors
    - `set_actor_transform(name, location, rotation, scale)` - Modify actor transform
    - `get_actor_properties(name="", actor_path="", include_components=True, detailed_components=True, world_type="auto")` - Get actor properties
    
    ## Blueprint Management
    - `create_blueprint(name, parent_class)` - Create new Blueprint classes
    - `add_component_to_blueprint(blueprint_name, component_type, component_name)` - Add components
    - `set_static_mesh_properties(blueprint_name, component_name, static_mesh)` - Configure meshes
    - `set_physics_properties(blueprint_name, component_name)` - Configure physics
    - `compile_blueprint(blueprint_name)` - Compile Blueprint changes
    - `cleanup_blueprint_for_reparent(blueprint_name, remove_components=[], remove_member_nodes=[], refresh_nodes=True, compile=True, save=True)` - Remove stale component/member nodes after reparenting
    - `set_blueprint_property(blueprint_name, property_name, property_value)` - Set properties
    - `set_pawn_properties(blueprint_name)` - Configure Pawn settings
    - `set_game_mode_default_pawn(game_mode_name, pawn_blueprint_name)` - Set the default pawn class for a GameMode Blueprint
    - `spawn_blueprint_actor(blueprint_name, actor_name)` - Spawn Blueprint actors

    ## Asset Tools
    - `search_assets(path="/Game", query="", class_name="", recursive_paths=True, include_tags=False, limit=50)` - Search assets
    - `get_asset_metadata(asset_path="", object_path="", asset_name="", name="")` - Query asset metadata, tags and references
    - `get_asset_dependencies(asset_path="", object_path="", asset_name="", name="")` - Query detailed dependencies
    - `get_asset_referencers(asset_path="", object_path="", asset_name="", name="")` - Query detailed referencers
    - `get_asset_summary(asset_path="", object_path="", asset_name="", name="")` - Load asset and summarize by type; WidgetBlueprint summaries include detailed widget layout data when available
    - `create_asset(name, asset_class, path="/Game", factory_class="", parent_class="", unique_name=False, save_asset=True)` - Create a generic asset with a UE factory; when `factory_class` is omitted, built-in defaults cover common asset classes such as Curve/Material/Texture/LevelSequence
    - `save_asset(asset_path="", object_path="", asset_name="", name="", only_if_dirty=False)` - Save an asset resolved by path or name
    - `import_asset(destination_path, filename="", source_files=[], destination_name="", async_import=True)` - Import external files into the Content Browser; currently uses async import to avoid UE5.7 Interchange sync assertions
    - `export_asset(export_path, asset_path="", asset_paths=[], clean_filenames=True)` - Export one or more assets to an external directory
    - `reimport_asset(asset_path="", asset_paths=[], ...)` - Reimport existing assets from their recorded source files; currently uses async reimport to avoid UE5.7 Interchange sync assertions
    - `fixup_redirectors(directory_path="", directory_paths=[], asset_path="", asset_paths=[], ...)` - Fix redirector references; returns idempotent success when none are found
    - `get_blueprint_summary(asset_path="", object_path="", asset_name="", name="")` - Load Blueprint asset and summarize structure; WidgetBlueprint results include `widget_tree`, slot/layout data, `bindings` and `animations`
    - `create_material_function(name, path="/Game/Materials", function_class="MaterialFunction")` - Create Material Function / Layer / Layer Blend assets
    - `create_render_target(name, path="/Game/RenderTargets", width=1024, height=1024, format="RTF_RGBA16F", clear_color=None, auto_generate_mips=False)` - Create Texture Render Target 2D assets
    
    ## Blueprint Node Management
    - `add_blueprint_event_node(blueprint_name, event_type)` - Add event nodes
    - `add_blueprint_input_action_node(blueprint_name, action_name)` - Add input nodes
    - `add_blueprint_function_node(blueprint_name, target, function_name)` - Add function nodes
    - `spawn_blueprint_node(blueprint_name, node_kind=None, node_class=None, ...)` - Spawn generic/special Blueprint nodes
    - `describe_blueprint_node(blueprint_name, node_id)` - Inspect node details and pins
    - `set_blueprint_pin_default(blueprint_name, node_id, pin_name, default_value=None, object_path=None)` - Set pin defaults
    - `delete_blueprint_node(blueprint_name, node_id)` - Delete a Blueprint node
    - `connect_blueprint_nodes(blueprint_name, source_node_id, source_pin, target_node_id, target_pin)` - Connect nodes
    - `add_blueprint_variable(blueprint_name, variable_name, variable_type)` - Add variables
    - `add_blueprint_get_self_component_reference(blueprint_name, component_name)` - Add component refs
    - `add_blueprint_self_reference(blueprint_name)` - Add self references
    - `find_blueprint_nodes(blueprint_name, node_type=None, event_type=None, include_details=False)` - Find nodes, supports Event/Function/Variable/InputAction/All
    
    ## Project Tools
    - `create_input_mapping(action_name, key, input_type)` - Create input mappings

    ## Metadata Tools
    - `list_mcp_tools(group="", name_contains="", include_parameters=False)` - List currently registered UnrealMCP tools
    - `export_tool_schema(tool_name="", group="")` - Export the current UnrealMCP tool input schemas

    ## Best Practices
    
    ### UMG Widget Development
    - Create widgets with descriptive names that reflect their purpose
    - Use consistent naming conventions for widget components
    - Organize widget hierarchy logically
    - Set appropriate anchors and alignment for responsive layouts
    - Use property bindings for dynamic updates instead of direct setting
    - Handle widget events appropriately with meaningful function names
    - Clean up widgets when no longer needed
    - Test widget layouts at different resolutions
    
    ### Editor and Actor Management
    - Use unique names for actors to avoid conflicts
    - Clean up temporary actors
    - Validate transforms before applying
    - Check actor existence before modifications
    - Take regular viewport screenshots during development
    - Keep the viewport focused on relevant actors during operations
    
    ### Blueprint Development
    - Compile Blueprints after changes
    - Use meaningful names for variables and functions
    - Organize nodes logically
    - Test functionality in isolation
    - Consider performance implications
    - Document complex setups

    ### Asset Inspection
    - Prefer `search_assets` and `get_asset_metadata` for low-cost discovery
    - Use `get_asset_summary` only when you need semantic details from a loaded asset
    - Use `get_blueprint_summary` for Blueprint-specific structure instead of generic summaries
    - For WidgetBlueprint layout inspection, prefer `get_blueprint_summary` or `get_asset_summary` and read the returned `widget_tree` plus slot/layout fields
    - Check dependencies and referencers before changing shared assets

    ### Error Handling
    - Check command responses for success
    - Handle errors gracefully
    - Log important operations
    - Validate parameters
    - Clean up resources on errors
    """

# Run the server
if __name__ == "__main__":
    logger.info("Starting MCP server with stdio transport")
    mcp.run(transport='stdio') 
