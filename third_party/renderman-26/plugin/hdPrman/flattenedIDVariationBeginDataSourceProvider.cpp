//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "hdPrman/flattenedIDVariationBeginDataSourceProvider.h"
#include "hdPrman/flattenedIDVariationDataSourceBase.h"
#include "hdPrman/primvarIDVariationSchema.h"

#include "pxr/imaging/hd/primvarSchema.h"

#include "pxr/base/trace/trace.h"

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

/// \class _FlattenedIDVariationBeginDataSource
///
/// A container data source that accumulates values from a parent data source.
///
/// It is instantiated from a data source containing the primvars of the prim
/// in question (conforming to HdPrimvarsSchema) and a flattened primvars data
/// source for the parent prim.
///
class _FlattenedIDVariationBeginDataSource :
    public HdPrman_FlattenedIDVariationDataSourceBase
{
public:
    HD_DECLARE_DATASOURCE(_FlattenedIDVariationBeginDataSource);
    
protected:
    /// Uncached version of _Get(), implementing the logic to check the parent
    /// data source for an ID variation primvar.
    HdContainerDataSourceHandle _GetUncached(const TfToken &name) override;

    /// Returns true if the token is the name of a primvar we're accumulating.
    bool _IsIDVariationToken(const TfToken &name) const override;

private:
    _FlattenedIDVariationBeginDataSource(
        const HdContainerDataSourceHandle &dataSource,
        const Handle &parentDataSource);
    
    HdContainerDataSourceHandle _UpdateAssemblyDepth();
    
    HdContainerDataSourceHandle _UpdateSubcomponentID();

    HdContainerDataSourceHandle _UpdateVariationIdCodes();    
};

_FlattenedIDVariationBeginDataSource::_FlattenedIDVariationBeginDataSource(
    const HdContainerDataSourceHandle &dataSource,
    const Handle &parentDataSource) :
    HdPrman_FlattenedIDVariationDataSourceBase(dataSource,
                                               parentDataSource)
{
}

// override
HdContainerDataSourceHandle
_FlattenedIDVariationBeginDataSource::_GetUncached(const TfToken &name)
{
    // Inherit the parent's assemblyDepth and increment if this prim
    // has an assemblyDepth as well.
    if (name == HdPrmanPrimvarIDVariationSchemaTokens->assemblyDepth) 
        return _UpdateAssemblyDepth();

    // Accumulate subcomponentID from parent to child.
    if (name == HdPrmanPrimvarIDVariationSchemaTokens->subcomponentID) 
        return _UpdateSubcomponentID();
    
    // Accumulate variationIDCodes from parent to child.
    if (name == HdPrmanPrimvarIDVariationSchemaTokens->variationIdCodes) 
        return _UpdateVariationIdCodes();

    return nullptr;
}

// override
bool
_FlattenedIDVariationBeginDataSource::
_IsIDVariationToken(const TfToken &name) const
{
    static const std::set<TfToken> tokenSet({
            HdPrmanPrimvarIDVariationSchemaTokens->assemblyDepth,
            HdPrmanPrimvarIDVariationSchemaTokens->subcomponentID,
            HdPrmanPrimvarIDVariationSchemaTokens->variationIdCodes
        });

    return tokenSet.count(name) > 0;
}

HdContainerDataSourceHandle
_FlattenedIDVariationBeginDataSource::_UpdateAssemblyDepth()
{
    TRACE_FUNCTION();

    int depth = 0;
    const HdPrman_FlattenedIDVariationDataSourceBaseHandle &parentDataSource =
        _GetParentDataSource();
    
    if (parentDataSource) {
        // Get the assemblyDepth from the parent.
        HdPrmanPrimvarIDVariationSchema parentSchema(parentDataSource);
        
        if (HdIntDataSourceHandle parentAssemblyDepthDs =
            parentSchema.GetAssemblyDepth()) {
            depth += parentAssemblyDepthDs->GetTypedValue(0.0f);
        }
    }

    const HdContainerDataSourceHandle &dataSource = _GetDataSource();
    if (dataSource) {
        // If this prim has an assemblyDepth, increment the total depth.
        HdPrmanPrimvarIDVariationSchema schema(dataSource);
        if (schema.GetAssemblyDepth())
            depth++;
    }
    
    if (depth > 0) {
        // Set the new assemblyDepth value.
        return HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<int>::New(depth))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build();
    }
    return nullptr;
}    

HdContainerDataSourceHandle
_FlattenedIDVariationBeginDataSource::_UpdateSubcomponentID()
{
    TRACE_FUNCTION();
    
    VtArray<VtIntArray> accumSubcomponentIDTable;
    const HdPrman_FlattenedIDVariationDataSourceBaseHandle &parentDataSource =
        _GetParentDataSource();

    if (parentDataSource) {
        HdPrmanPrimvarIDVariationSchema parentSchema(parentDataSource);
        
        // Get the subcomponentID from the parent.
        if (HdSampledDataSourceHandle parentSubcomponentIDDs =
            parentSchema.GetSubcomponentID()) {
            
            // We need to cast to a VtValue and then get a VtArray<VtIntArray>,
            // since this primvar is not stored in a typed data source.
            VtValue value =
                VtValue::Cast< VtArray<VtIntArray> >(
                    parentSubcomponentIDDs->GetValue(0.0f));
            if (!value.IsEmpty()) {
                const VtArray<VtIntArray> &parentSubcomponentIDTable =
                    value.UncheckedGet< VtArray<VtIntArray> >();
                accumSubcomponentIDTable.assign(
                    parentSubcomponentIDTable.begin(),
                    parentSubcomponentIDTable.end());
            }
        }
    }

    const HdContainerDataSourceHandle &dataSource = _GetDataSource();
    if (dataSource) {
        // Get subcomponentID from this prim's data source and append them to
        // the result.
        HdPrmanPrimvarIDVariationSchema schema(dataSource);
        
        if (HdSampledDataSourceHandle subcomponentIDDs =
            schema.GetSubcomponentID()) {
            VtValue value =
                VtValue::Cast< VtArray<VtIntArray> >(
                    subcomponentIDDs->GetValue(0.0f));
            if (!value.IsEmpty()) {
                const VtArray<VtIntArray> &subcomponentIDTable =
                    value.UncheckedGet< VtArray<VtIntArray> >();
                std::copy(subcomponentIDTable.begin(),
                          subcomponentIDTable.end(),
                          std::back_inserter(accumSubcomponentIDTable));
            }
        }
    }
    
    if (!accumSubcomponentIDTable.empty()) {
        // Set the new subcomponentID value.
        return HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource< VtArray<VtIntArray> >::New(
                    accumSubcomponentIDTable))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build();    
    }
    return nullptr;
}

HdContainerDataSourceHandle
_FlattenedIDVariationBeginDataSource::_UpdateVariationIdCodes()
{
    TRACE_FUNCTION();
    
    VtFloatArray accumCodesArray;
    const HdPrman_FlattenedIDVariationDataSourceBaseHandle &parentDataSource =
        _GetParentDataSource();

    if (parentDataSource) {
        HdPrmanPrimvarIDVariationSchema parentSchema(parentDataSource);
        
        // Get the variationIdCodes from the parent.
        if (HdFloatArrayDataSourceHandle parentVariationIdCodesDs =
            parentSchema.GetVariationIdCodes()) {
            const VtFloatArray &parentCodesArray =
                parentVariationIdCodesDs->GetTypedValue(0.0f);
            accumCodesArray.assign(parentCodesArray.begin(),
                                   parentCodesArray.end());
        }
    }

    const HdContainerDataSourceHandle &dataSource = _GetDataSource();
    if (dataSource) {
        // Get variationIdCodes from this prim's data source and append them to
        // the result.
        HdPrmanPrimvarIDVariationSchema schema(dataSource);
        if (HdFloatArrayDataSourceHandle variationIdCodesDs =
            schema.GetVariationIdCodes()) {
            const VtFloatArray &codesArray =
                variationIdCodesDs->GetTypedValue(0.0f);
            int code = static_cast<int>(floor(codesArray.back()));
            accumCodesArray.push_back(code);
        }
    }
    
    if (!accumCodesArray.empty()) {
        // Set the new variationIdCodes value.
        return HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                    accumCodesArray))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->constant))
            .Build();
    }
    return nullptr;
}

HdContainerDataSourceHandle
HdPrman_FlattenedIDVariationBeginDataSourceProvider::GetFlattenedDataSource(
    const Context &ctx) const
{
    return _FlattenedIDVariationBeginDataSource::New(
        ctx.GetInputDataSource(),
        _FlattenedIDVariationBeginDataSource::Cast(
            ctx.GetFlattenedDataSourceFromParentPrim()));
}

void
HdPrman_FlattenedIDVariationBeginDataSourceProvider::
ComputeDirtyLocatorsForDescendants(HdDataSourceLocatorSet *locators) const
{
    *locators = HdDataSourceLocatorSet::UniversalSet();
}

PXR_NAMESPACE_CLOSE_SCOPE
