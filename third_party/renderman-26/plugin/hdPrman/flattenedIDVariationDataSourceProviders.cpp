//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "hdPrman/flattenedIDVariationBeginDataSourceProvider.h"
#include "hdPrman/flattenedIDVariationEndDataSourceProvider.h"
#include "hdPrman/primvarIDVariationSchema.h"

PXR_NAMESPACE_OPEN_SCOPE

using namespace HdMakeDataSourceContainingFlattenedDataSourceProvider;

// Functions used as inputs to HdFlatteningSceneIndex.

HdContainerDataSourceHandle
HdPrman_FlattenedIDVariationBeginDataSourceProviders()
{
    static const HdContainerDataSourceHandle result =
        HdRetainedContainerDataSource::New(
            HdPrmanPrimvarIDVariationSchema::GetSchemaToken(),
            Make<HdPrman_FlattenedIDVariationBeginDataSourceProvider>());

    return result;
}

HdContainerDataSourceHandle
HdPrman_FlattenedIDVariationEndDataSourceProviders()
{
    static const HdContainerDataSourceHandle result =
        HdRetainedContainerDataSource::New(
            HdPrmanPrimvarIDVariationSchema::GetSchemaToken(),
            Make<HdPrman_FlattenedIDVariationEndDataSourceProvider>());

    return result;
}

PXR_NAMESPACE_CLOSE_SCOPE
