//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_DATA_SOURCE_PROVIDERS_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_DATA_SOURCE_PROVIDERS_H

#include "pxr/imaging/hd/dataSource.h"

PXR_NAMESPACE_OPEN_SCOPE

/// Contains the flattened data source provider for the first stage of
/// ID variation (accumulating primvar values from parent to child).
///
/// Can be given as inputArgs to the HdFlatteningSceneIndex.
///
HdContainerDataSourceHandle
HdPrman_FlattenedIDVariationBeginDataSourceProviders();

/// Contains the flattened data source provider for the last stage of
/// ID variation (inheriting primvars from parent to child).
///
/// Can be given as inputArgs to the HdFlatteningSceneIndex.
///
HdContainerDataSourceHandle
HdPrman_FlattenedIDVariationEndDataSourceProviders();


PXR_NAMESPACE_CLOSE_SCOPE

#endif // EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_DATA_SOURCE_PROVIDERS_H
