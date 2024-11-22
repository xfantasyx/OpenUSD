//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_DATA_SOURCE_BASE_H
#define EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_DATA_SOURCE_BASE_H

#include "pxr/imaging/hd/invalidatableContainerDataSource.h"

#include <tbb/concurrent_hash_map.h>

#include <set>

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdPrman_FlattenedIDVariationDataSourceBase
///
/// A data source base class that can be used to inherit or acccumulate values
/// from a parent data source.
///
/// It is instantiated from a data source containing the primvars of the prim
/// in question (conforming to HdPrimvarsSchema) and a flattened primvars data
/// source for the parent prim. There is a cache of name/value pairs for
/// efficiency.
///
/// (Mostly swiped from _PrimvarsDataSource, which is used by
/// HdFlattenedPrimvarsDataSourceProvider to inherit constant primvars.)
///
class HdPrman_FlattenedIDVariationDataSourceBase :
    public HdInvalidatableContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE_ABSTRACT(HdPrman_FlattenedIDVariationDataSourceBase);

    virtual ~HdPrman_FlattenedIDVariationDataSourceBase();
    
    /// Queries prim's data source for a primvar. If it's an ID variation
    /// primvar, queries the parent's data source and accumulates the result.
    HdDataSourceBaseHandle Get(const TfToken &name) override;

    /// Get the names of the ID variation primvars.
    std::shared_ptr< std::set<TfToken> > GetIDVariationPrimvarNames();
    
    /// Adds the names of the parent's ID variation primvars to this prim's
    /// primvars.
    TfTokenVector GetNames() override;

    /// Invalidate specific cached primvars.
    bool Invalidate(const HdDataSourceLocatorSet &locators) override;

protected:
    HdPrman_FlattenedIDVariationDataSourceBase(
        const HdContainerDataSourceHandle &dataSource,
        const Handle &parentDataSource);

    const HdContainerDataSourceHandle &_GetDataSource() const {
        return _dataSource;
    }

    const Handle &_GetParentDataSource() const {
        return _parentDataSource;
    }

    /// Uncached version of _Get(), implementing the logic to check the parent
    /// data source for an ID variation primvar. Must be implemented by
    /// derived classes.
    virtual HdContainerDataSourceHandle _GetUncached(const TfToken &name) = 0;

    /// Returns true if the token is the name of an ID variation primvar
    /// handled by this data source. Must be implemented by derived classes.
    virtual bool _IsIDVariationToken(const TfToken &name) const = 0;
    
private:
    std::set<TfToken> _GetIDVariationPrimvarNamesUncached();
    
    struct _TokenHashCompare {
        static bool equal(const TfToken &a,
                          const TfToken &b) {
            return a == b;
        }
        
        static size_t hash(const TfToken &a) {
            return hash_value(a);
        }
    };

    using _NameToPrimvarDataSource = 
        tbb::concurrent_hash_map<
            TfToken,
            HdDataSourceBaseAtomicHandle,
            _TokenHashCompare>;
    
    const HdContainerDataSourceHandle _dataSource;
    const Handle _parentDataSource;

    // Cached data sources
    //
    // We store a base rather than a container so we can distinguish between
    // the absence of a cached value (nullptr) and a cached value that might
    // be indicating that the primvar exists (can cast to HdContainerDataSource)
    // or not exist (stored as bool data source).
    _NameToPrimvarDataSource _nameToPrimvarDataSource;

    // Cached ID variation primvar names
    std::shared_ptr< std::set<TfToken> > _primvarNames;
};

HD_DECLARE_DATASOURCE_HANDLES(HdPrman_FlattenedIDVariationDataSourceBase);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // EXT_RMANPKG_PLUGIN_RENDERMAN_PLUGIN_HD_PRMAN_FLATTENED_ID_VARIATION_DATA_SOURCE_BASE_H
