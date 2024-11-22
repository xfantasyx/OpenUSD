//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_BEGIN_SCENE_INDEX_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_BEGIN_SCENE_INDEX_H

#include "pxr/pxr.h"
#if PXR_VERSION >= 2308

#include "pxr/imaging/hd/filteringSceneIndex.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(HdPrman_IDVariationBeginSceneIndex);

/// \class HdPrman_IDVariationBeginSceneIndex
///
/// This scene index is the first step for handling ID variation in Hydra.
/// (Porting functionality from studio_genVariationIDs in Katana.)
/// It generates random IDs for each type of prim, setting these attributes:
///
/// assemblyID
/// componentID
/// hasIDVariation
/// ri:attributes:user:componentID
/// ri:attributes:user:gprimID
///
/// It also sets initial values for these attributes:
///
/// assemblyDepth
/// subcomponentID
/// variationIdCodes
///
/// HdPrman_FlattenedIDVariationBeginDataSourceProvider, which is run as part
/// of a flattening scene index, will accumulate values from parent to child for
/// the above attributes.
///
/// HdPrman_IDVariationEndSceneIndex will process the attributes set in earlier
/// steps and set additional attributes.
///
/// HdPrman_FlattenedIDVariationEndDataSourceProvider, which is run as part
/// of a flattening scene index, will handle inheritance of values from parent
/// to child for the above attributes.
///
/// HdPrman_IDVariationSceneIndexPlugin bundles the above scene indices, so
/// they're run in the correct order.
///
class HdPrman_IDVariationBeginSceneIndex : 
    public  HdSingleInputFilteringSceneIndexBase
{
public:
    static HdPrman_IDVariationBeginSceneIndexRefPtr
    New(const HdSceneIndexBaseRefPtr& inputSceneIndex);

    HdSceneIndexPrim GetPrim(const SdfPath &primPath) const override;

    SdfPathVector GetChildPrimPaths(const SdfPath &primPath) const override;

protected:
    HdPrman_IDVariationBeginSceneIndex(
        const HdSceneIndexBaseRefPtr& inputSceneIndex);
    
    ~HdPrman_IDVariationBeginSceneIndex();

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

#endif //EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_ID_VARIATION_BEGIN_SCENE_INDEX_H
