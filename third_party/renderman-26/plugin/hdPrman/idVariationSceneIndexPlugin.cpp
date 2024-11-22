//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "hdPrman/idVariationSceneIndexPlugin.h"

#if PXR_VERSION >= 2308

#include "hdPrman/flattenedIDVariationDataSourceProviders.h"
#include "hdPrman/idVariationBeginSceneIndex.h"
#include "hdPrman/idVariationEndSceneIndex.h"
#include "hdPrman/tokens.h"

#include "pxr/imaging/hd/flatteningSceneIndex.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/sceneIndexPluginRegistry.h"
#include "pxr/imaging/hd/tokens.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName, "HdPrman_IDVariationSceneIndexPlugin"))
);

TF_REGISTRY_FUNCTION(TfType)
{
    HdSceneIndexPluginRegistry
        ::Define<HdPrman_IDVariationSceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    // XXX insertionPhase cannot be greater than 1 for this plugin.
    const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase = 1;

    for (auto const& rendererDisplayName : HdPrman_GetPluginDisplayNames()) {
        HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
            rendererDisplayName,
            _tokens->sceneIndexPluginName,
            nullptr, // No input args.
            insertionPhase,
            HdSceneIndexPluginRegistry::InsertionOrderAtStart);
    }
}

HdPrman_IDVariationSceneIndexPlugin::HdPrman_IDVariationSceneIndexPlugin() =
    default;

HdSceneIndexBaseRefPtr
HdPrman_IDVariationSceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene,
    const HdContainerDataSourceHandle &inputArgs)
{
    // HdPrman_IDVariationBeginSceneIndex generates IDs and sets initial
    // primvars. We flatten immediately after to accumulate values that
    // are needed by HdPrman_IDVariationEndSceneIndex.
    HdSceneIndexBaseRefPtr sceneIndex =
        HdFlatteningSceneIndex::New(
            HdPrman_IDVariationBeginSceneIndex::New(inputScene),
            HdPrman_FlattenedIDVariationBeginDataSourceProviders());

    // HdPriman_IDVariationEndSceneIndex sets attributes based on the
    // values accumulated in the first step. We flatten again to ensure
    // the primvars we added are inherited from parent to child.
    //
    // XXX Maybe there should be a more general-purpose utility for this?
    sceneIndex =
        HdFlatteningSceneIndex::New(
            HdPrman_IDVariationEndSceneIndex::New(sceneIndex),
            HdPrman_FlattenedIDVariationEndDataSourceProviders());

    return sceneIndex;
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_VERSION >= 2308
