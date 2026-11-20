#pragma once

#include "chromascope/AsyncCameraCapture.hpp"
#include "chromascope/PublicStreamPolicy.hpp"

#include <memory>
#include <atomic>
#include <string_view>

namespace chromascope {

enum class PublicStreamMediaKind { Auto, Snapshot, HlsMpegTs, Mp4 };

// Constructed on the GUI thread after an explicit Start action. Construction
// validates the URL but does not perform DNS resolution or network I/O.
// AsyncCameraCapture owns and calls read() on the existing capture worker.
[[nodiscard]] std::unique_ptr<CameraFrameReader> make_public_stream_reader(
    std::string_view https_url, std::shared_ptr<std::atomic<bool>> cancel = {},
    PublicStreamMediaKind media_kind = PublicStreamMediaKind::Auto,
    std::string_view local_playlist_file = {});

} // namespace chromascope
