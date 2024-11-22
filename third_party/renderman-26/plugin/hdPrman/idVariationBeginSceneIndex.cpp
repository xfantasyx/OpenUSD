//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "hdPrman/idVariationBeginSceneIndex.h"
#include "hdPrman/primvarIDVariationSchema.h"

#if PXR_VERSION >= 2308

#include "pxr/usdImaging/usdImaging/usdPrimInfoSchema.h"

#include "pxr/usd/kind/registry.h"
#include "pxr/usd/sdf/path.h"

#include "pxr/imaging/hd/containerDataSourceEditor.h"
#include "pxr/imaging/hd/dataSourceLocator.h"
#include "pxr/imaging/hd/dataSourceTypeDefs.h"
#include "pxr/imaging/hd/filteringSceneIndex.h"
#include "pxr/imaging/hd/instancerTopologySchema.h"
#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/primvarSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/base/trace/trace.h"

#include "ext/lava/general/hashFuncSdbm/hashFuncSdbm.h"

#include "amber/pxCoreSchema/tokens.h"

#include <random>

PXR_NAMESPACE_OPEN_SCOPE

// static
// Returns a random ID, given a path and offset.
static int
_ComputeID(const SdfPath &path, int offset = 0)
{
    static int idMin = 100000;
    static int idMax = 999999;

    int seed = hashFuncSdbm_getHash(path.GetAsString());

    std::default_random_engine generator(seed + offset);
    std::uniform_int_distribution<int> distribution(idMin, idMax);

    return distribution(generator);
}

// static
// Adds random IDs to idArray, given a path. The number of IDs is specified
// by numIDs.
static void
_ComputeIDArray(const SdfPath &path, size_t numIDs, VtIntArray *idArray)
{
    idArray->reserve(idArray->size() + numIDs);
    
    for (size_t i = 0; i < numIDs; i++) {
        idArray->push_back(_ComputeID(path, i));
    }
}

/// \class _IDVariationBeginDataSource
///
/// This data source returns a custom primvars data source when queried.
/// 
/// (Mostly swiped from HdPrman_UpdateObjectSettings_DataSource, which is used
/// by HdPrman_UpdateObjectSettingsSceneIndex.)
///
class _IDVariationBeginDataSource: public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_IDVariationBeginDataSource);

    TfTokenVector GetNames() override;
    
    HdDataSourceBaseHandle Get(const TfToken &name) override;

protected:
    _IDVariationBeginDataSource(const HdContainerDataSourceHandle &inputDs,
                                const TfToken &inputPrimType,
                                const SdfPath &inputPrimPath);

private:
    HdDataSourceBaseHandle _UpdatePrimvars();
    
    HdContainerDataSourceHandle _inputPrimDs;
    const TfToken _inputPrimType;
    const SdfPath _inputPrimPath;
};


_IDVariationBeginDataSource::_IDVariationBeginDataSource(
    const HdContainerDataSourceHandle &inputDs,
    const TfToken &inputPrimType,
    const SdfPath &inputPrimPath) :
    _inputPrimDs(inputDs),
    _inputPrimType(inputPrimType),
    _inputPrimPath(inputPrimPath)
{
}

HdDataSourceBaseHandle
_IDVariationBeginDataSource::Get(const TfToken &name)
{
    HdDataSourceBaseHandle result;
    
    if (name == HdPrmanPrimvarIDVariationSchemaTokens->primvars) 
        result = _UpdatePrimvars();
    
    if (!result) 
        result = _inputPrimDs->Get(name);
    
    return result;
}

TfTokenVector
_IDVariationBeginDataSource::GetNames()
{
    return _inputPrimDs->GetNames();
}

HdDataSourceBaseHandle
_IDVariationBeginDataSource::_UpdatePrimvars()
{
    TRACE_FUNCTION();

    HdPrmanPrimvarIDVariationSchema schema =
        HdPrmanPrimvarIDVariationSchema::GetFromParent(_inputPrimDs);    

    // If we have a dustID, we don't need to do anything else.
    if (schema && schema.GetDustID()) 
        return nullptr;

    // Determine the 'kind'.
    TfToken kind;
    if (UsdImagingUsdPrimInfoSchema primInfoSchema =
        UsdImagingUsdPrimInfoSchema::GetFromParent(_inputPrimDs)) {
        if (HdTokenDataSourceHandle kindDs = primInfoSchema.GetKind()) 
            kind = kindDs->GetTypedValue(0.0f);
    }

    // Skip any FX assets.
    if (kind == PxCoreSchemaKindsTokens->fxAsset) 
        return nullptr;

    // If this is a point instancer, determine the number of instances.
    size_t numInstances = 0;
    if (HdInstancerTopologySchema instancerSchema =
        HdInstancerTopologySchema::GetFromParent(_inputPrimDs)) {
        // Instance indices are encoded as an array of arrays: Each element in
        // the outer array corresponds to a prototype. Each prototype has an
        // array specifying the indices where that prototype is used. We count
        // up the elements in the inner arrays to determine the number of
        // instances.
        if (HdIntArrayVectorSchema indicesSchema =
            instancerSchema.GetInstanceIndices()) {
            for (size_t i = 0; i < indicesSchema.GetNumElements(); i++) {
                if (HdIntArrayDataSourceHandle indicesDs =
                    indicesSchema.GetElement(i)) {
                    const VtIntArray &indices = indicesDs->GetTypedValue(0.0f);
                    numInstances += indices.size();
                }
            }
        }
    }

    // Create a primvars container data source if we don't already have one.
    HdContainerDataSourceHandle container;
    if (schema)
        container = schema.GetContainer();
    else {
        container =
            HdRetainedContainerDataSource::New(
                HdPrmanPrimvarIDVariationSchemaTokens->primvars,
                HdRetainedContainerDataSource::New());
    }    
    HdContainerDataSourceEditor primvarsEditor(container);

    int id = 0;
    VtIntArray idArray;
    HdPrmanPrimvarIDVariationSchemaCodes code =
        HdPrmanPrimvarIDVariationSchemaCodes::GPRIM;
    
    if (kind == PxCoreSchemaKindsTokens->dress_group) {
        // Vector Component
        code = HdPrmanPrimvarIDVariationSchemaCodes::COMPONENTV;

        // Generate IDs for each instance and set the componentID attribute.
        _ComputeIDArray(_inputPrimPath, numInstances, &idArray);
        
        primvarsEditor.Overlay(
            HdDataSourceLocator(
                HdPrmanPrimvarIDVariationSchemaTokens->componentID),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<VtIntArray>::New(idArray))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build());        
    }
    else if (KindRegistry::IsA(kind, KindTokens->assembly)) {
        // Assembly
        code = HdPrmanPrimvarIDVariationSchemaCodes::ASSEMBLY;

        // Generate an ID and set the assemblyID attribute.
        id = _ComputeID(_inputPrimPath);
        primvarsEditor.Overlay(
            HdDataSourceLocator(
                HdPrmanPrimvarIDVariationSchemaTokens->assemblyID),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<int>::New(id))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build());

        // Set the assemblyDepth to 0. It will get incremented later in
        // HdPrman_FlattenedIDVariationBeginDataSourceProvider, which takes
        // into account ancestor prims.
        int depth = 0;
        primvarsEditor.Overlay(
            HdDataSourceLocator(
                HdPrmanPrimvarIDVariationSchemaTokens->assemblyDepth),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<int>::New(depth))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build());
    }
    else if (KindRegistry::IsA(kind, KindTokens->component)) {
        // Component
        code = HdPrmanPrimvarIDVariationSchemaCodes::COMPONENT;

        // The geo scope in an instance source is a component and should be
        // ignored.
        static const TfToken geo("geo");
        if (_inputPrimPath.GetNameToken() != geo) {
            // Generate an ID and set the corresponding rman attribute.
            id = _ComputeID(_inputPrimPath);

            primvarsEditor.Overlay(
                HdDataSourceLocator(
                    HdPrmanPrimvarIDVariationSchemaRiTokens->componentID),
                HdPrimvarSchema::Builder()
                .SetPrimvarValue(
                    HdRetainedTypedSampledDataSource<int>::New(id))
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->constant))
                .Build());
        }
    }
    else if (KindRegistry::IsA(kind, KindTokens->subcomponent)) {
        if (_inputPrimType == HdPrimTypeTokens->instancer) {
            // Vector Subcomponent
            code = HdPrmanPrimvarIDVariationSchemaCodes::SUBCOMPONENTV;

            // Generate IDs for each instance and add them to the table.
            _ComputeIDArray(_inputPrimPath, numInstances, &idArray);
        }
        else { 
            // Subcomponent
            code = HdPrmanPrimvarIDVariationSchemaCodes::SUBCOMPONENT;

            // Generate an ID and add it to the table.
            id = _ComputeID(_inputPrimPath);
            idArray.push_back(id);
        }

        // subcomponentID is an array of arrays: Each entry in the outer array
        // is vector of IDs. We add idArray here. Later, in
        // HdPrman_FlattenedIDVariationBeginDataSourceProvider, ID vectors from
        // ancestor subcomponents will get prepended.
        VtArray<VtIntArray> subcomponentIDTable(1, idArray);
        primvarsEditor.Overlay(
            HdDataSourceLocator(
                HdPrmanPrimvarIDVariationSchemaTokens->subcomponentID),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource< VtArray<VtIntArray> >::New(
                    subcomponentIDTable))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build());
    }
    else if (schema) {
        // Gprim
        // Check for a gprimID primvar. If it exists, copy its value to the
        // rman attribute.
        if (HdIntDataSourceHandle intDs = schema.GetGprimID()) {
            id = intDs->GetTypedValue(0.0f);
            
            primvarsEditor.Overlay(
                HdDataSourceLocator(
                    HdPrmanPrimvarIDVariationSchemaRiTokens->gprimID),
                HdPrimvarSchema::Builder()
                .SetPrimvarValue(
                    HdRetainedTypedSampledDataSource<int>::New(id))
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->constant))
                .Build());
        }
    }

    if ((id > 0) || !idArray.empty()) {
        // If we found or generated any IDs, we set hasIDVariation.
        // This is used by HdPrman_IDVariationEndSceneIndex to output
        // additional attributes on the prims with IDs.
        bool hasIDVariation = true;
        primvarsEditor.Overlay(
            HdDataSourceLocator(
                HdPrmanPrimvarIDVariationSchemaTokens->hasIDVariation),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<bool>::New(
                    hasIDVariation))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build());

        // Prim types have a numerical code that is used by the shader for
        // variation. We add this prim's code here. Later, in
        // HdPrman_FlattenedIDVariationBeginDataSourceProvider, codes from
        // ancestor prims will get prepended.
        VtFloatArray codesArray(1, static_cast<float>(code));
        primvarsEditor.Overlay(
            HdDataSourceLocator(
                HdPrmanPrimvarIDVariationSchemaTokens->variationIdCodes),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<VtFloatArray>::New(codesArray))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build());
    }
    
    return primvarsEditor.Finish();
}

// static
HdPrman_IDVariationBeginSceneIndexRefPtr
HdPrman_IDVariationBeginSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(  
        new HdPrman_IDVariationBeginSceneIndex(inputSceneIndex));
}

HdPrman_IDVariationBeginSceneIndex::HdPrman_IDVariationBeginSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
{
}

HdSceneIndexPrim 
HdPrman_IDVariationBeginSceneIndex::GetPrim(const SdfPath &primPath) const
{
    const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);

    // If this prim's data source exists and it's under the anim or sets
    // scopes, we create a new data source that can add ID variation primvars.
    static const SdfPath animPath("/World/anim");
    static const SdfPath setsPath("/World/sets");

    if (prim.dataSource &&
        (primPath.HasPrefix(animPath) || primPath.HasPrefix(setsPath))) {
        return {
            prim.primType,
            _IDVariationBeginDataSource::New(prim.dataSource,
                                             prim.primType,
                                             primPath)
        };
    }
    return prim;
}

SdfPathVector 
HdPrman_IDVariationBeginSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
HdPrman_IDVariationBeginSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{    
    _SendPrimsAdded(entries);
}

void 
HdPrman_IDVariationBeginSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    _SendPrimsRemoved(entries);
}

void
HdPrman_IDVariationBeginSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    // Note that changing 'kind' is not supported.
    _SendPrimsDirtied(entries);
}

HdPrman_IDVariationBeginSceneIndex::~HdPrman_IDVariationBeginSceneIndex() =
    default;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_VERSION >= 2308
