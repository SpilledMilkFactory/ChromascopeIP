#include "chromascope/AnnotationView.hpp"

#include <variant>
#include <vector>

namespace chromascope {

bool annotation_visible(const Annotation& annotation,
                        const AnnotationViewOptions& options) noexcept {
    // Avoid an optimizer-dependent unreachable tail in MSVC's instantiated
    // std::visit visitor (C4702 under /WX). Preserve all three shape filters.
    if (std::holds_alternative<CircleGeometry>(annotation.geometry))
        return options.circles;
    if (std::holds_alternative<BoxGeometry>(annotation.geometry))
        return options.boxes;
    if (std::holds_alternative<PolygonGeometry>(annotation.geometry))
        return options.polygons;
    return false; // No geometry is visible if the variant is valueless.
}

AnnotationLayer make_annotation_view(const AnnotationLayer& source,
                                    const AnnotationViewOptions& options) {
    std::vector<Annotation> visible;
    visible.reserve(source.items().size());
    for (std::size_t i = 0; i < source.items().size(); ++i) {
        const auto& item = source.items()[i];
        if (!annotation_visible(item, options)) continue;
        if (options.selected_only && options.selected_index != i) continue;
        visible.push_back(item);
        if (options.selected_index == i) {
            // Explicit gold selection tint is presentation-only; original
            // Skittles stroke colors remain unchanged in the source layer.
            visible.back().stroke = {255, 191, 0};
        }
    }
    return AnnotationLayer(source.width(), source.height(), std::move(visible));
}

} // namespace chromascope
