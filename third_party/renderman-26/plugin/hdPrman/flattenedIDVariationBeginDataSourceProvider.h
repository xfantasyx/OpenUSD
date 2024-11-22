//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_BEGIN_DATA_SOURCE_PROVIDER_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_BEGIN_DATA_SOURCE_PROVIDER_H

#include "pxr/imaging/hd/flattenedDataSourceProvider.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdPrman_FlattenedIDVariationBeginDataSourceProvider
///
/// Used in combination with an HdFlatteningSceneIndex to accumulate values
/// from parent to child. These attributes are modified:
///
/// assemblyDepth (incremented based on the depth of the assembly prim)
/// subcomponentID (accumulated from ancestors)
/// variationIdCodes (accumulated from ancestors)
///
class HdPrman_FlattenedIDVariationBeginDataSourceProvider :
    public HdFlattenedDataSourceProvider
{
    HdContainerDataSourceHandle GetFlattenedDataSource(
        const Context&) const override;

    void ComputeDirtyLocatorsForDescendants(
        HdDataSourceLocatorSet *locators) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_BEGIN_DATA_SOURCE_PROVIDER_H
