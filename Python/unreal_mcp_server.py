"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import socket
import sys
import json
import time
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp.log'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("UnrealMCP")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557
DEFAULT_SOCKET_TIMEOUT_SECONDS = 5.0
LIVE_CODING_WAIT_TIMEOUT_SECONDS = 120.0
LIVE_CODING_STATE_FALLBACK_TIMEOUT_SECONDS = 5.0
LIVE_CODING_STATE_FALLBACK_POLL_SECONDS = 15.0
LIVE_CODING_STATE_FALLBACK_POLL_INTERVAL_SECONDS = 0.5
BLUEPRINT_COMMAND_TIMEOUT_SECONDS = 20.0

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
    
    def connect(self, timeout_seconds: float = DEFAULT_SOCKET_TIMEOUT_SECONDS) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
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
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(
        self,
        sock,
        buffer_size=4096,
        timeout_seconds: float = DEFAULT_SOCKET_TIMEOUT_SECONDS
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
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    logger.debug(f"Received partial response, waiting for more data...")
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning(f"Socket timeout during receive after {timeout_seconds:.1f}s")
            if chunks:
                data = b''.join(chunks)
                try:
                    decoded_data = data.decode('utf-8')
                    json.loads(decoded_data)
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except UnicodeDecodeError:
                    logger.debug("Buffered response ended with incomplete UTF-8 sequence")
                except Exception:
                    pass
            raise Exception(f"Timeout receiving Unreal response after {timeout_seconds:.1f}s")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise

    def _get_response_timeout_seconds(self, command: str, params: Dict[str, Any]) -> float:
        """Resolve a command-specific response timeout."""
        if command == "compile_live_coding" and params.get("wait_for_completion"):
            return LIVE_CODING_WAIT_TIMEOUT_SECONDS
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
        # Always reconnect for each command, since Unreal closes the connection after each command
        # This is different from Unity which keeps connections alive
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        command_params = params or {}
        timeout_seconds = response_timeout_seconds
        if timeout_seconds is None:
            timeout_seconds = self._get_response_timeout_seconds(command, command_params)

        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        
        try:
            # Match Unity's command format exactly
            command_obj = {
                "type": command,  # Use "type" instead of "command"
                "params": command_params  # Use Unity's params or {} pattern
            }
            
            # Send without newline, exactly like Unity
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            
            # Read response using improved handler
            response_data = self.receive_full_response(
                self.socket,
                timeout_seconds=timeout_seconds
            )
            response = json.loads(response_data.decode('utf-8'))
            
            # Log complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check for both error formats: {"status": "error", ...} and {"success": false, ...}
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                # We want to preserve the original error structure but ensure error is accessible
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                # This format uses {"success": false, "error": "message"} or {"success": false, "message": "message"}
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                # Convert to the standard format expected by higher layers
                response = {
                    "status": "error",
                    "error": error_message
                }
            
            # Always close the connection after command is complete
            # since Unreal will close it on its side anyway
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            error_message = str(e)

            if (allow_live_coding_timeout_fallback and
                command == "compile_live_coding" and
                command_params.get("wait_for_completion") and
                "Timeout receiving Unreal response" in error_message):
                live_coding_state = self._query_live_coding_state_after_timeout()
                response: Dict[str, Any] = {
                    "status": "error",
                    "error": error_message,
                    "timed_out_waiting_for_response": True
                }
                if live_coding_state is not None:
                    response["live_coding_state"] = live_coding_state.get("result", live_coding_state)
                return response

            # Always reset connection state on any error
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                _unreal_connection = None
        
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
        _unreal_connection = get_unreal_connection()
        if _unreal_connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
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

# Register tools
register_editor_tools(mcp)
register_asset_tools(mcp)
register_blueprint_tools(mcp)
register_blueprint_node_tools(mcp)
register_project_tools(mcp)
register_umg_tools(mcp)  

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
    - `save_asset(asset_path="", object_path="", asset_name="", name="", only_if_dirty=False)` - Save an asset resolved by path or name
    - `get_blueprint_summary(asset_path="", object_path="", asset_name="", name="")` - Load Blueprint asset and summarize structure; WidgetBlueprint results include `widget_tree`, slot/layout data, `bindings` and `animations`
    
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
