//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "hdPrman/flattenedIDVariationEndDataSourceProvider.h"
#include "hdPrman/flattenedIDVariationDataSourceBase.h"
#include "hdPrman/primvarIDVariationSchema.h"

#include "pxr/base/tf/stringUtils.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class _FlattenedIDVariationEndDataSource
///
/// A container data source that inherits values from a parent data source.
///
/// It is instantiated from a data source containing the primvars of the prim
/// in question (conforming to HdPrimvarsSchema) and a flattened primvars data
/// source for the parent prim.
///
class _FlattenedIDVariationEndDataSource :
    public HdPrman_FlattenedIDVariationDataSourceBase
{
public:
    HD_DECLARE_DATASOURCE(_FlattenedIDVariationEndDataSource);

protected:
    /// Uncached version of _Get(), implementing the logic to check the parent
    /// data source for an ID variation primvar.
    HdContainerDataSourceHandle _GetUncached(const TfToken &name) override;

    /// Returns true if the token is the name of a primvar we're inheriting.
    bool _IsIDVariationToken(const TfToken &name) const override;

private:
    _FlattenedIDVariationEndDataSource(
        const HdContainerDataSourceHandle &dataSource,
        const Handle &parentDataSource);
};

_FlattenedIDVariationEndDataSource::_FlattenedIDVariationEndDataSource(
    const HdContainerDataSourceHandle &dataSource,
    const Handle &parentDataSource) :
    HdPrman_FlattenedIDVariationDataSourceBase(dataSource,
                                               parentDataSource)
{
}

// override
HdContainerDataSourceHandle
_FlattenedIDVariationEndDataSource::_GetUncached(const TfToken &name)
{
    HdContainerDataSourceHandle result;
    
    // Check if this prim has the primvar.
    const HdContainerDataSourceHandle &dataSource = _GetDataSource();
    if (dataSource) 
        result = HdContainerDataSource::Cast(dataSource->Get(name));
    
    // Otherwise, check the parent.
    if (!result) {
        const
            HdPrman_FlattenedIDVariationDataSourceBaseHandle &parentDataSource =
            _GetParentDataSource();
        if (parentDataSource) 
            result = HdContainerDataSource::Cast(parentDataSource->Get(name));
    }
    return result;
}

// override
bool
_FlattenedIDVariationEndDataSource::
_IsIDVariationToken(const TfToken &name) const
{
    // First check for an exact match with one of these tokens.
    static const std::set<TfToken> tokenMatchSet({
            HdPrmanPrimvarIDVariationSchemaTokens->assemblyID,
            HdPrmanPrimvarIDVariationSchemaTokens->componentID,
            HdPrmanPrimvarIDVariationSchemaTokens->variationIdCodes,
            HdPrmanPrimvarIDVariationSchemaRiTokens->componentID,
            HdPrmanPrimvarIDVariationSchemaRiTokens->gprimID
        });

    if (tokenMatchSet.count(name) > 0)
        return true;

    // Otherwise, check if 'name' contains one of these prefixes.
    static const std::vector<TfToken> tokenPrefixVec({
            HdPrmanPrimvarIDVariationSchemaTokens->subcomponentID,
            HdPrmanPrimvarIDVariationSchemaRiTokens->assemblyID,
            HdPrmanPrimvarIDVariationSchemaRiTokens->subcomponentID
        });
    
    const std::string &nameStr = name.GetString();
    for (const TfToken &prefix : tokenPrefixVec) {
        if (TfStringStartsWith(nameStr, prefix.GetString()))
            return true;
    }
    
    return false;
}

HdContainerDataSourceHandle
HdPrman_FlattenedIDVariationEndDataSourceProvider::GetFlattenedDataSource(
    const Context &ctx) const
{
    return _FlattenedIDVariationEndDataSource::New(
        ctx.GetInputDataSource(),
        _FlattenedIDVariationEndDataSource::Cast(
            ctx.GetFlattenedDataSourceFromParentPrim()));
}

void
HdPrman_FlattenedIDVariationEndDataSourceProvider::
ComputeDirtyLocatorsForDescendants(HdDataSourceLocatorSet *locators) const
{
    *locators = HdDataSourceLocatorSet::UniversalSet();
}

PXR_NAMESPACE_CLOSE_SCOPE
