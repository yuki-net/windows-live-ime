#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "windows_live_ime/core/model.hpp"

#include <cstddef>
#include <span>

namespace windows_live_ime::renderer {

class CandidateRenderer {
public:
    virtual ~CandidateRenderer() = default;

    [[nodiscard]] virtual HRESULT show(
        HWND owner_window,
        std::span<const core::Candidate> candidates,
        std::size_t selected_index) = 0;
    virtual void hide() noexcept = 0;
};

}  // namespace windows_live_ime::renderer
