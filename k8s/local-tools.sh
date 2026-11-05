#!/bin/sh
# Use only this checkout's isolated tools and cluster credentials.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "${1:-}" in
  kubectl|minikube) tool=$1; shift ;;
  *) echo "Usage: sh k8s/local-tools.sh kubectl|minikube [arguments...]" >&2; exit 2 ;;
esac
binary="$root/.deployment-local/bin/$tool"
if [ ! -x "$binary" ]; then
  echo "Missing local tool: $binary. See k8s/README.md for prerequisites." >&2
  exit 1
fi
export MINIKUBE_HOME="$root/.deployment-local/minikube"
export KUBECONFIG="$root/.deployment-local/kube/config"
export PATH="$root/.deployment-local/bin:/usr/bin:/bin"
if [ "$tool" = minikube ] && ! docker info >/dev/null 2>&1; then
  # Drop back to the invoking user with temporary Docker-group access.
  # Do not run Minikube as root or change permanent group/socket permissions.
  exec pkexec /usr/bin/runuser --user "$(id -un)" --supp-group docker -- \
    /usr/bin/env HOME="$HOME" MINIKUBE_HOME="$MINIKUBE_HOME" \
    KUBECONFIG="$KUBECONFIG" PATH="$PATH" "$binary" "$@"
fi
exec "$binary" "$@"
