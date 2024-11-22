//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "hdPrman/idVariationEndSceneIndex.h"
#include "hdPrman/debugCodes.h"
#include "hdPrman/primvarIDVariationSchema.h"

#if PXR_VERSION >= 2308

#include "pxr/usd/sdf/path.h"

#include "pxr/imaging/hd/containerDataSourceEditor.h"
#include "pxr/imaging/hd/dataSourceLocator.h"
#include "pxr/imaging/hd/dataSourceTypeDefs.h"
#include "pxr/imaging/hd/filteringSceneIndex.h"
#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/primvarSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/base/tf/debug.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/trace/trace.h"

#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    HDPRMAN_ID_VARIATION
);

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(HDPRMAN_ID_VARIATION, "ID variation");
}

// static
// Erases the primvars specified in eraseVec.
static void
_ErasePrimvars(HdContainerDataSourceEditor &primvarsEditor,
               const TfTokenVector &eraseVec)
{
    // Allow temporary attributes to be examined if the debug flag is set.
    if (TfDebug::IsEnabled(HDPRMAN_ID_VARIATION))
        return;

    for (size_t i = 0; i < eraseVec.size(); i++) {
        primvarsEditor.Set(HdDataSourceLocator(eraseVec[i]),
                           HdBlockDataSource::New());
    }
}

/// \class _IDVariationEndDataSource
///
/// This data source returns a custom primvars data source when queried.
///
/// (Mostly swiped from HdPrman_UpdateObjectSettings_DataSource, which is used
/// by HdPrman_UpdateObjectSettingsSceneIndex.)
///
class _IDVariationEndDataSource: public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_IDVariationEndDataSource);

    TfTokenVector GetNames() override;
    
    HdDataSourceBaseHandle Get(const TfToken &name) override;

protected:
    _IDVariationEndDataSource(const HdContainerDataSourceHandle &inputDs);

private:
    HdDataSourceBaseHandle _UpdatePrimvars();
    
    HdContainerDataSourceHandle _inputPrimDs;
};


_IDVariationEndDataSource::_IDVariationEndDataSource(
    const HdContainerDataSourceHandle &inputDs) :
    _inputPrimDs(inputDs)
{
}

HdDataSourceBaseHandle
_IDVariationEndDataSource::Get(const TfToken &name)
{
    HdDataSourceBaseHandle result;
    
    if (name == HdPrmanPrimvarIDVariationSchemaTokens->primvars) 
        result = _UpdatePrimvars();
    
    if (!result) 
        result = _inputPrimDs->Get(name);
    
    return result;
}

TfTokenVector
_IDVariationEndDataSource::GetNames()
{
    return _inputPrimDs->GetNames();
}

HdDataSourceBaseHandle
_IDVariationEndDataSource::_UpdatePrimvars()
{
    TRACE_FUNCTION();
    
    HdPrmanPrimvarIDVariationSchema schema =
        HdPrmanPrimvarIDVariationSchema::GetFromParent(_inputPrimDs);
    
    if (!schema)
        return nullptr;
    
    // If variationIdCodes doesn't exist, we don't need to do anything.
    VtFloatArray codesArray;
    if (HdFloatArrayDataSourceHandle variationIdCodesDs =
        schema.GetVariationIdCodes()) {
        codesArray = variationIdCodesDs->GetTypedValue(0.0f);
    }    
    if (codesArray.empty())
        return nullptr;
    
    HdContainerDataSourceEditor primvarsEditor(schema.GetContainer());

    // assemblyDepth, hasIDVariation, and subcomponentID are temporary
    // attributes that store accumulated values. They can be erased when
    // we're done. 
    TfTokenVector eraseVec{
        HdPrmanPrimvarIDVariationSchemaTokens->assemblyDepth,
        HdPrmanPrimvarIDVariationSchemaTokens->hasIDVariation,
        HdPrmanPrimvarIDVariationSchemaTokens->subcomponentID};

    // variationIdCodes stores codes that are accumulated from parent to child.
    // We need to output these for gprims, but they can be erased for all
    // other types.
    HdPrmanPrimvarIDVariationSchemaCodes code =
        static_cast<HdPrmanPrimvarIDVariationSchemaCodes>(
            floor(codesArray.back()));
    
    if (code != HdPrmanPrimvarIDVariationSchemaCodes::GPRIM) {
        eraseVec.push_back(
            HdPrmanPrimvarIDVariationSchemaTokens->variationIdCodes);
    }

    // If hasIDVariation isn't set, we just need to clean up temporary
    // attributes.
    bool hasIDVariation = false;
    if (HdBoolDataSourceHandle hasIDVariationDs = schema.HasIDVariation())
        hasIDVariation = hasIDVariationDs->GetTypedValue(0.0f);

    if (!hasIDVariation) {
        _ErasePrimvars(primvarsEditor, eraseVec);
        return primvarsEditor.Finish();
    }

    switch(code) {
    case HdPrmanPrimvarIDVariationSchemaCodes::ASSEMBLY:
    {
        // assemblyDepth is the number of assembly prims in the hierarchy.
        if (HdIntDataSourceHandle assemblyDepthDs = schema.GetAssemblyDepth()) {
            int assemblyDepth = assemblyDepthDs->GetTypedValue(0.0f);
            
            // assemblyID is the unique ID for this prim.
            if (HdIntDataSourceHandle assemblyIDDs = schema.GetAssemblyID()) {
                int id = assemblyIDDs->GetTypedValue(0.0f);
                
                // The name of the rman attribute is based on the assemblyDepth.
                const TfToken attrName =
                    TfToken(TfStringPrintf(
                                "%s%d",
                                HdPrmanPrimvarIDVariationSchemaRiTokens->
                                assemblyID.GetText(),
                                assemblyDepth));
                
                primvarsEditor.Overlay(
                    HdDataSourceLocator(attrName),
                    HdPrimvarSchema::Builder()
                    .SetPrimvarValue(
                        HdRetainedTypedSampledDataSource<int>::New(id))
                    .SetInterpolation(
                        HdPrimvarSchema::BuildInterpolationDataSource(
                            HdPrimvarSchemaTokens->constant))
                    .Build());
            }
        }
    }
    break;
    case HdPrmanPrimvarIDVariationSchemaCodes::SUBCOMPONENT:
    {
        // subcomponentID contains a unique ID for this subcomponent and its
        // ancestor subcomponents.
        if (HdSampledDataSourceHandle subcomponentIDDs =
            schema.GetSubcomponentID()) {
            // We need to cast to a VtValue and then get a VtArray<VtIntArray>,
            // since this primvar is not stored in a typed data source.
            VtValue value =
                VtValue::Cast< VtArray<VtIntArray> >(
                    subcomponentIDDs->GetValue(0.0f));
            if (!value.IsEmpty()) {
                const VtArray<VtIntArray> &subcomponentIDTable =
                    value.UncheckedGet< VtArray<VtIntArray> >();

                // The last entry in the array is the ID for this subcomponent.
                const VtIntArray &subcomponentIDArray =
                    subcomponentIDTable.back();
                int id = subcomponentIDArray.back();
                
                // The name of the rman attribute is based on the number of
                // subcomponents in the hierarchy.
                const TfToken attrName =
                    TfToken(TfStringPrintf(
                                "%s%zu",
                                HdPrmanPrimvarIDVariationSchemaRiTokens->
                                subcomponentID.GetText(),
                                subcomponentIDTable.size()));
                        
                primvarsEditor.Overlay(
                    HdDataSourceLocator(attrName),
                    HdPrimvarSchema::Builder()
                    .SetPrimvarValue(
                        HdRetainedTypedSampledDataSource<int>::New(id))
                    .SetInterpolation(
                        HdPrimvarSchema::BuildInterpolationDataSource(
                            HdPrimvarSchemaTokens->constant))
                    .Build());
            }
        }
    }
    break;
    case HdPrmanPrimvarIDVariationSchemaCodes::SUBCOMPONENTV:
    {
        // subcomponentID contains unique IDs for each instance of this
        // subcomponent, as well its ancestor subcomponents.
        if (HdSampledDataSourceHandle subcomponentIDDs =
            schema.GetSubcomponentID()) {
            // We need to cast to a VtValue and then get a VtArray<VtIntArray>,
            // since this primvar is not stored in a typed data source.
            VtValue value =
                VtValue::Cast< VtArray<VtIntArray> >(
                    subcomponentIDDs->GetValue(0.0f));
            if (!value.IsEmpty()) {
                const VtArray<VtIntArray> &subcomponentIDTable =
                    value.UncheckedGet< VtArray<VtIntArray> >();

                // The name of the attribute is based on the number of
                // subcomponents in the hierarchy.
                const TfToken attrName =
                    TfToken(TfStringPrintf(
                                "%s%zu",
                                HdPrmanPrimvarIDVariationSchemaTokens->
                                subcomponentID.GetText(),
                                subcomponentIDTable.size()));
                        
                primvarsEditor.Overlay(
                    HdDataSourceLocator(attrName),
                    HdPrimvarSchema::Builder()
                    .SetPrimvarValue(
                        HdRetainedTypedSampledDataSource<
                        VtArray<VtIntArray> >::New(subcomponentIDTable))
                    .SetInterpolation(
                        HdPrimvarSchema::BuildInterpolationDataSource(
                            HdPrimvarSchemaTokens->constant))
                    .Build());
            }
        }
    }
    case HdPrmanPrimvarIDVariationSchemaCodes::GPRIM:
    {
        // The shader expects variationIdCodes to be a fixed-length array,
        // where unused elements are set to -1. We reformat the array here.
        static size_t codesArrayMaxSize = 16;
        static float padValue = -1.0;
            
        if (codesArray.size() < codesArrayMaxSize) {
            codesArray.resize(codesArrayMaxSize, padValue);
            
            primvarsEditor.Overlay(
                HdDataSourceLocator(
                    HdPrmanPrimvarIDVariationSchemaTokens->variationIdCodes),
                HdPrimvarSchema::Builder()
                .SetPrimvarValue(
                    HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                        codesArray))
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->constant))
                .Build());
        }
    }
    break;
    default:
        break;
    }

    // Clean up temporary attributes.
    _ErasePrimvars(primvarsEditor, eraseVec);
    
    return primvarsEditor.Finish();
}

// static
HdPrman_IDVariationEndSceneIndexRefPtr
HdPrman_IDVariationEndSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(  
        new HdPrman_IDVariationEndSceneIndex(inputSceneIndex));
}

HdPrman_IDVariationEndSceneIndex::HdPrman_IDVariationEndSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
{
}

HdSceneIndexPrim 
HdPrman_IDVariationEndSceneIndex::GetPrim(const SdfPath &primPath) const
{
    const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
    if (prim.dataSource) {
        return {
            prim.primType,
            _IDVariationEndDataSource::New(prim.dataSource)
        };
    }
    return prim;
}

SdfPathVector 
HdPrman_IDVariationEndSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
HdPrman_IDVariationEndSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{    
    _SendPrimsAdded(entries);
}

void 
HdPrman_IDVariationEndSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    _SendPrimsRemoved(entries);
}

void
HdPrman_IDVariationEndSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    // Note that changing 'kind' is not supported.
    _SendPrimsDirtied(entries);
}

HdPrman_IDVariationEndSceneIndex::~HdPrman_IDVariationEndSceneIndex() = default;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_VERSION >= 2308
