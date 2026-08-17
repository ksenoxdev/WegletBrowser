// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_url_loader_factory.cc

#include "weglet/browser/weglet_url_loader_factory.h"

#include <string>
#include <utility>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/memory/self_deleting.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "weglet/ui/generated_resources.h"

namespace weglet {
namespace {

// weglet://newtab/ -> "newtab.html", weglet://newtab/base.css -> "base.css".
//
// The host names the page and the path names a file next to it. Both are
// flat and matched exactly against the embedded set, so there is nothing
// here for "..", a percent-encoded slash or a symlink to reach -- a
// directory traversal has no directory to walk. Returns an empty string
// for anything that is not that shape.
std::string ResourcePathFor(const GURL& url) {
  const std::string path(url.path());
  if (path.empty() || path == "/") {
    return std::string(url.host()) + ".html";
  }
  // One leading slash and nothing else: a further separator means this was
  // not a request for a file beside the page.
  const std::string relative = path.substr(1);
  if (relative.find('/') != std::string::npos ||
      relative.find('\\') != std::string::npos) {
    return std::string();
  }
  return relative;
}

class WegletURLLoaderFactory : public network::SelfDeletingURLLoaderFactory {
 public:
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create() {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
    // MakeSelfDeleting, not new: the base class takes a pass key that only
    // this helper can produce, which is how it stops anyone allocating one
    // by hand and side-stepping its lifetime management. The object frees
    // itself when its last receiver disconnects.
    base::MakeSelfDeleting<WegletURLLoaderFactory>(
        remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  WegletURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      base::SelfDeletingPassKey key)
      : network::SelfDeletingURLLoaderFactory(std::move(receiver), key) {}

  WegletURLLoaderFactory(const WegletURLLoaderFactory&) = delete;
  WegletURLLoaderFactory& operator=(const WegletURLLoaderFactory&) = delete;

 private:
  // Private on purpose, and MakeSelfDeleting static_asserts on it: nobody
  // outside may delete this.
  ~WegletURLLoaderFactory() override = default;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
      const net::MutableNetworkTrafficAnnotationTag& annotation) override {
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(client_remote));

    const std::string path = ResourcePathFor(request.url);
    const ui::Resource* resource = path.empty() ? nullptr : ui::Find(path);
    if (!resource) {
      // One of our pages asking for something that is not embedded is a
      // build mistake, not a runtime condition to paper over.
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FILE_NOT_FOUND));
      return;
    }

    const std::string mime_type(resource->mime_type);

    auto head = network::mojom::URLResponseHead::New();
    head->mime_type = mime_type;
    head->charset = "utf-8";
    head->content_length = static_cast<int64_t>(resource->contents.size());
    // Belt and braces with the CSP in the pages themselves: a weglet://
    // page has no business being framed by anything.
    head->headers = net::HttpResponseHeaders::TryToCreate(
        "HTTP/1.1 200 OK\nX-Frame-Options: DENY\nContent-Type: " + mime_type +
        "; charset=utf-8\n\n");

    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    // Sized for the whole file plus one byte, so the single write below
    // always fits and there is no partial-write path to get wrong.
    if (mojo::CreateDataPipe(resource->contents.size() + 1, producer,
                             consumer) != MOJO_RESULT_OK) {
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }

    client->OnReceiveResponse(std::move(head), std::move(consumer),
                              std::nullopt);

    // WriteAllData rather than WriteData: it returns OK only when every
    // byte went in, so a short write is an error instead of a page that
    // silently loads half its stylesheet.
    const MojoResult result =
        producer->WriteAllData(base::as_byte_span(resource->contents));

    network::URLLoaderCompletionStatus status(
        result == MOJO_RESULT_OK ? net::OK : net::ERR_FAILED);
    // base::ByteSize, not int64_t: its constructor is explicit and takes an
    // unsigned integer, which size() already is.
    const base::ByteSize size(resource->contents.size());
    status.encoded_data_length = size;
    status.encoded_body_length = size;
    status.decoded_body_length = size;
    client->OnComplete(status);
  }
};

}  // namespace

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWegletURLLoaderFactory() {
  return WegletURLLoaderFactory::Create();
}

}  // namespace weglet
