//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
/// \file glPlatformContext.cpp

#include "pxr/imaging/garch/glPlatformContext.h"
#include "pxr/base/tf/hash.h"
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE


//
// GarchGLXContextState
//

GarchAndroidContextState::GarchAndroidContextState() :

    context(eglGetCurrentContext()),
    display(eglGetDisplay(EGL_DEFAULT_DISPLAY)),
    draw(eglGetCurrentSurface(EGL_DRAW)),
    _defaultCtor(true)
{
    // std::cout << "GarchAndroidContextState::GarchAndroidContextState without params" << std::endl;
    // Do nothing
}

GarchAndroidContextState::GarchAndroidContextState(EGLDisplay display_, EGLSurface draw_, EGLContext context_) :
    display(display_),
    draw(draw_),
    context(context_),
    _defaultCtor(false)
{
    // std::cout << "GarchAndroidContextState::GarchAndroidContextState with params" << std::endl;
    // Do nothing
}

bool GarchAndroidContextState::operator==(const GarchAndroidContextState& rhs) const
{
    std::cout << "GarchAndroidContextState::==" << (display == rhs.display && draw == rhs.draw && context == rhs.context) << std::endl;
    return display == rhs.display && draw == rhs.draw && context == rhs.context;
}

size_t GarchAndroidContextState::GetHash() const
{
    return TfHash::Combine(display, draw, context);
}

bool GarchAndroidContextState::IsValid() const
{
    std::cout << "GarchAndroidContextState::IsValid\ndisplay:" << (display && true) << "\ndraw: " << (draw && true)
              << "\ncontext:" << (context && true) << std::endl;
    return display && draw && context;
}

void GarchAndroidContextState::MakeCurrent()
{
    // std::cout << "GarchAndroidContextState::MakeCurrent" << std::endl;
    if (IsValid())
    {
        eglMakeCurrent(display, draw, draw, context);
    }
    else if (_defaultCtor)
    {
        DoneCurrent();
    }
}

void GarchAndroidContextState::DoneCurrent()
{
    // std::cout << "GarchAndroidContextState::DoneCurrent" << std::endl;
    if (EGLDisplay display = eglGetCurrentDisplay())
    {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
}

GarchAndroidContextState GarchGetNullGLPlatformContextState()
{
    // std::cout << "GarchAndroidContextState::GarchGetNullGLPlatformContextState" << std::endl;
    return GarchAndroidContextState(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

PXR_NAMESPACE_CLOSE_SCOPE
