//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_END_DATA_SOURCE_PROVIDER_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_END_DATA_SOURCE_PROVIDER_H

#include "pxr/imaging/hd/flattenedDataSourceProvider.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdPrman_FlattenedIDVariationEndDataSourceProvider
///
/// Used in combination with an HdFlatteningSceneIndex to handle inheritance
/// of values from parent to child. These attribute values are inherited:
///
/// assemblyID
/// componentID
/// ri:attributes:user:assemblyID${DEPTH}
/// ri:attributes:user:componentID
/// ri:attributes:user:gprimID
/// ri:attributes:user:subcomponentID${DEPTH}
/// subcomponentID${DEPTH}
/// variationIdCodes
///
class HdPrman_FlattenedIDVariationEndDataSourceProvider :
    public HdFlattenedDataSourceProvider
{
    HdContainerDataSourceHandle GetFlattenedDataSource(
        const Context&) const override;

    void ComputeDirtyLocatorsForDescendants(
        HdDataSourceLocatorSet *locators) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_END_DATA_SOURCE_PROVIDER_H
