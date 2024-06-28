#!/usr/bin/env bash
set -e

# Increase the pip timeout to handle TimeoutError
export PIP_DEFAULT_TIMEOUT=200

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT=$DIR/../
cd $ROOT

echo "installing uv..."
curl -LsSf https://astral.sh/uv/install.sh | sh
UV_BIN='$HOME/.cargo/env'
ADD_PATH_CMD=". \"$UV_BIN\""
eval $ADD_PATH_CMD

# TODO: remove --no-cache once this is fixed: https://github.com/astral-sh/uv/issues/4378
echo "installing python packages..."
uv --no-cache sync --all-extras
source .venv/bin/activate

echo "PYTHONPATH=${PWD}" > $ROOT/.env
if [[ "$(uname)" == 'Darwin' ]]; then
  echo "# msgq doesn't work on mac" >> $ROOT/.env
  echo "export ZMQ=1" >> $ROOT/.env
  echo "export OBJC_DISABLE_INITIALIZE_FORK_SAFETY=YES" >> $ROOT/.env
fi

if [ "$(uname)" != "Darwin" ] && [ -e "$ROOT/.git" ]; then
  echo "pre-commit hooks install..."
  pre-commit install
  git submodule foreach pre-commit install
fi
