#include "chromascope/AnnotationView.hpp"

#include <type_traits>
#include <variant>
#include <vector>

namespace chromascope {

bool annotation_visible(const Annotation& annotation,
                        const AnnotationViewOptions& options) noexcept {
    return std::visit([&options](const auto& geometry) {
        using Shape = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<Shape, CircleGeometry>) return options.circles;
        if constexpr (std::is_same_v<Shape, BoxGeometry>) return options.boxes;
        return options.polygons;
    }, annotation.geometry);
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
