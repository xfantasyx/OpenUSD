//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "hdPrman/flattenedIDVariationDataSourceBase.h"

PXR_NAMESPACE_OPEN_SCOPE

HdPrman_FlattenedIDVariationDataSourceBase::
HdPrman_FlattenedIDVariationDataSourceBase(
    const HdContainerDataSourceHandle &dataSource,
    const Handle &parentDataSource) :
    _dataSource(dataSource),
    _parentDataSource(parentDataSource)
{
}

// virtual
HdPrman_FlattenedIDVariationDataSourceBase::
~HdPrman_FlattenedIDVariationDataSourceBase()
{
}

HdDataSourceBaseHandle
HdPrman_FlattenedIDVariationDataSourceBase::Get(const TfToken &name)
{
    // If this isn't an ID variation primvar, we just call Get().
    if (!_IsIDVariationToken(name)) {
        if (_dataSource)
            return _dataSource->Get(name);
        
        return nullptr;
    }

    // Otherwise, this is an ID variation primvar. First, check the cache.
    _NameToPrimvarDataSource::accessor a;
    _nameToPrimvarDataSource.insert(a, name);

    if (const HdDataSourceBaseHandle ds =
        HdDataSourceBase::AtomicLoad(a->second)) {
        // Cache hit.
        return ds;
    }
    
    // Cache miss.
    const HdContainerDataSourceHandle result = _GetUncached(name);
    HdDataSourceBase::AtomicStore(a->second, result);

    return result;
}

std::shared_ptr< std::set<TfToken> >
HdPrman_FlattenedIDVariationDataSourceBase::GetIDVariationPrimvarNames()
{
    std::shared_ptr< std::set<TfToken> > result =
        std::atomic_load(&_primvarNames);

    if (!result) {
        // Cache miss
        result = std::make_shared< std::set<TfToken> >(
            _GetIDVariationPrimvarNamesUncached());
        std::atomic_store(&_primvarNames, result);
    }

    return result;
}

TfTokenVector
HdPrman_FlattenedIDVariationDataSourceBase::GetNames()
{
    TfTokenVector result;
    // First get primvars from this prim.
    if (_dataSource) 
        result = _dataSource->GetNames();

    if (!_parentDataSource) 
        return result;

    // Get ID variation primvars from the parent.
    std::set<TfToken> idVariationPrimvars =
        *(_parentDataSource->GetIDVariationPrimvarNames());
    if (idVariationPrimvars.empty()) 
        return result;

    // To avoid duplicates, erase this prim's primvars.
    for (const TfToken &name : result) {
        idVariationPrimvars.erase(name);
    }

    // And add the primvars not already in the result.
    result.insert(result.end(),
                  idVariationPrimvars.begin(), idVariationPrimvars.end());
    
    return result;
}

bool
HdPrman_FlattenedIDVariationDataSourceBase::Invalidate(
    const HdDataSourceLocatorSet &locators)
{
    bool anyDirtied = false;

    // Iterate through all locators starting with 'primvars'. We only consider
    // the ID variation ones before checking if they exist in our cache.
    for (const HdDataSourceLocator &locator : locators) {
        const TfToken &primvarName = locator.GetFirstElement();
        if (_IsIDVariationToken(primvarName) &&
            _nameToPrimvarDataSource.erase(primvarName)) { 
            anyDirtied = true;
        }
    }

    return anyDirtied;
}

std::set<TfToken>
HdPrman_FlattenedIDVariationDataSourceBase::
_GetIDVariationPrimvarNamesUncached()
{
    std::set<TfToken> result;

    // Get ID variation primvars from the parent.
    if (_parentDataSource) 
        result = *_parentDataSource->GetIDVariationPrimvarNames();

    // Add primvars from this prim.
    if (_dataSource) {
        for (const TfToken &name : _dataSource->GetNames()) {
            if (_IsIDVariationToken(name)) 
                result.insert(name);
        }
    }
    return result;
}

PXR_NAMESPACE_CLOSE_SCOPE
