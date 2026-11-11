#pragma once

#include "chromascope/Annotation.hpp"

#include <cstddef>
#include <optional>

namespace chromascope {

// Non-destructive, presentation-only selection over an immutable annotation
// snapshot. Indices always address the ORIGINAL AnnotationLayer::items().
struct AnnotationViewOptions {
    bool circles = true;
    bool boxes = true;
    bool polygons = true;
    std::optional<std::size_t> selected_index;
    bool selected_only = false;
};

[[nodiscard]] bool annotation_visible(const Annotation& annotation,
                                      const AnnotationViewOptions& options) noexcept;

// Returns an owned layer without modifying the source, detection results,
// class labels, confidence values, or geometry coordinates. A selected item
// is highlighted only in the returned presentation layer.
[[nodiscard]] AnnotationLayer make_annotation_view(const AnnotationLayer& source,
                                                   const AnnotationViewOptions& options);

} // namespace chromascope
