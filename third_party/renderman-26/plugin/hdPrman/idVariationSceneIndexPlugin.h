//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_SCENE_INDEX_PLUGIN_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_SCENE_INDEX_PLUGIN_H

#include "pxr/pxr.h"
#if PXR_VERSION >= 2308

#include "pxr/imaging/hd/sceneIndexPlugin.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdPrman_IDVariationSceneIndexPlugin
///
/// Pixar-only, Prman-specific Hydra scene index to handle ID variation.
/// 
/// This plugin bundles the following scene indices and flattened data source
/// providers, so they're run in the correct order by the
/// HdSceneIndexPluginRegistry:
///
/// HdPrman_IDVariationBeginSceneIndex
/// HdPrman_FlattenedIDVariationBeginDataSourceProvider
/// HdPrman_IDVariationEndSceneIndex
/// HdPrman_FlattenedIDVariationEndDataSourceProvider
///
class HdPrman_IDVariationSceneIndexPlugin : public HdSceneIndexPlugin
{
public:
    HdPrman_IDVariationSceneIndexPlugin();

protected:
    HdSceneIndexBaseRefPtr _AppendSceneIndex(
        const HdSceneIndexBaseRefPtr &inputScene,
        const HdContainerDataSourceHandle &inputArgs) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_VERSION >= 2308

#endif // EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_SCENE_INDEX_PLUGIN_H
