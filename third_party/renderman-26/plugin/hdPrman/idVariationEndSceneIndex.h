//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_END_SCENE_INDEX_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_END_SCENE_INDEX_H

#include "pxr/pxr.h"
#if PXR_VERSION >= 2308

#include "pxr/imaging/hd/filteringSceneIndex.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(HdPrman_IDVariationEndSceneIndex);

/// \class HdPrman_IDVariationEndSceneIndex
///
/// This scene index is the finishing step for handling ID variation in Hydra.
/// (Porting functionality from studio_genVariationIDs in Katana.)
/// It sets attributes with names based on hierachy depth:
///
/// ri:attributes:user:assemblyID${DEPTH}
/// ri:attributes:user:subcomponentID${DEPTH}
/// subcomponentID${DEPTH}
///
/// It also erases the following attributes, which are only needed while
/// processing:
///
/// assemblyDepth
/// hasIDVariation
/// subcomponentID
/// variationIdCodes (erased on non-gprims)
///
/// HdPrman_IDVariationBeginSceneIndex should be called before this scene index
/// to generate IDs and populate initial values.
/// 
/// HdPrman_FlattenedIDVariationBeginDataSourceProvider will accumulate values
/// from parent to child, as part of a flattening scene index. This should
/// also be run before HdPrman_IDVariationEndSceneIndex.
///
/// HdPrman_FlattenedIDVariationEndDataSourceProvider is the last step, which
/// is run as part of a flattening scene index. It will handle inheritance of
/// values from parent to child.
///
/// HdPrman_IDVariationSceneIndexPlugin bundles the above scene indices and
/// flattened data source providers, so they're run in the correct order.
/// 
class HdPrman_IDVariationEndSceneIndex : 
    public  HdSingleInputFilteringSceneIndexBase
{
public:
    static HdPrman_IDVariationEndSceneIndexRefPtr
    New(const HdSceneIndexBaseRefPtr& inputSceneIndex);

    HdSceneIndexPrim GetPrim(const SdfPath &primPath) const override;

    SdfPathVector GetChildPrimPaths(const SdfPath &primPath) const override;

protected:
    HdPrman_IDVariationEndSceneIndex(
        const HdSceneIndexBaseRefPtr& inputSceneIndex);
    
    ~HdPrman_IDVariationEndSceneIndex();

    void _PrimsAdded(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::AddedPrimEntries &entries) override;

    void _PrimsRemoved(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    void _PrimsDirtied(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::DirtiedPrimEntries &entries) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_VERSION >= 2308

#endif //EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_END_SCENE_INDEX_H
