// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_devtools_manager_delegate.h"

#include <atomic>
#include <memory>

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"

namespace weglet {

namespace {

constexpr int kBackLog = 10;

std::atomic<int>& LastUsedPort() {
  static std::atomic<int> port{0};
  return port;
}

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory() = default;
  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    // Port 0: an OS-assigned ephemeral port, read back below. Loopback
    // only -- this is local file serving, not a remote-debugging endpoint.
    if (socket->ListenWithAddressAndPort(
            net::IPAddress::IPv4Localhost().ToString(), 0, kBackLog) !=
        net::OK) {
      return nullptr;
    }
    net::IPEndPoint endpoint;
    if (socket->GetLocalAddress(&endpoint) == net::OK) {
      LastUsedPort().store(endpoint.port(), std::memory_order_release);
    }
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }
};

}  // namespace

// static
void WegletDevToolsManagerDelegate::StartHttpHandler(
    content::BrowserContext* browser_context) {
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::make_unique<TCPServerSocketFactory>(), browser_context->GetPath(),
      base::FilePath());
}

// static
void WegletDevToolsManagerDelegate::StopHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

// static
int WegletDevToolsManagerDelegate::GetHttpHandlerPort() {
  return LastUsedPort().load(std::memory_order_acquire);
}

WegletDevToolsManagerDelegate::WegletDevToolsManagerDelegate() = default;
WegletDevToolsManagerDelegate::~WegletDevToolsManagerDelegate() = default;

bool WegletDevToolsManagerDelegate::HasBundledFrontendResources() {
  return true;
}

}  // namespace weglet
