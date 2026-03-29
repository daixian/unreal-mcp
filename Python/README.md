# Unreal MCP

Python bridge for interacting with the UnrealMCP Unreal Engine editor plugin using the Model Context Protocol (MCP).

## Setup

1. Make sure Python 3.10+ is installed
2. Install `uv` if you haven't already:
   ```bash
   curl -LsSf https://astral.sh/uv/install.sh | sh
   ```
3. Create and activate a virtual environment:
   ```bash
   uv venv
   source .venv/bin/activate  # On Unix/macOS
   # or
   .venv\Scripts\activate     # On Windows
   ```
4. Install dependencies:
   ```bash
   uv pip install -e .
   ```

At this point, you can configure your MCP client to use the Unreal MCP Server. See the repository root [README.md](../README.md) for an example configuration.

## Testing Scripts

There are several scripts in the [scripts](./scripts) folder. They are useful for testing the tools and the Unreal Bridge via a direct connection. This means that you do not need to have an MCP Server running.

You should make sure you have installed dependencies and/or are running in the `uv` virtual environment in order for the scripts to work.


## Troubleshooting

- Make sure Unreal Engine editor is loaded and running before running the server.
- Check logs in `unreal_mcp.log` for detailed error information

## Development

To add new Unreal-side commands, update the C++ bridge and command handlers under `../Source/UnrealMCP`.

To expose those commands through MCP, add or update the matching Python tool module under `tools/`, then register it in `unreal_mcp_server.py`.

Current Python tool modules include editor, blueprint, blueprint node, project, UMG, and asset tools.
