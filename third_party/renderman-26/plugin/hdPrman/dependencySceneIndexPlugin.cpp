//
// Copyright 2022 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "hdPrman/dependencySceneIndexPlugin.h"

#include "hdPrman/tokens.h"

#include "pxr/imaging/hd/containerDataSourceEditor.h"
#include "pxr/imaging/hd/dataSource.h"
#include "pxr/imaging/hd/dataSourceLocator.h"
#include "pxr/imaging/hd/dataSourceTypeDefs.h"
#include "pxr/imaging/hd/dependenciesSchema.h"
#include "pxr/imaging/hd/dependencySchema.h"
#include "pxr/imaging/hd/filteringSceneIndex.h"
#include "pxr/imaging/hd/lazyContainerDataSource.h"
#include "pxr/imaging/hd/lightSchema.h"
#include "pxr/imaging/hd/mapContainerDataSource.h"
#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/sceneIndex.h"
#include "pxr/imaging/hd/sceneIndexObserver.h"
#include "pxr/imaging/hd/sceneIndexPluginRegistry.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/visibilitySchema.h"
#include "pxr/imaging/hd/volumeFieldBindingSchema.h"
#include "pxr/imaging/hd/volumeFieldSchema.h"

#include "pxr/usd/sdf/path.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/vt/value.h"

#include "pxr/pxr.h"

#include <cstddef>
#include <functional>
#include <vector>

#if PXR_VERSION >= 2405
#include "pxr/imaging/hd/collectionsSchema.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName, "HdPrman_DependencySceneIndexPlugin"))
    (__dependenciesToFilters)

    // Legacy; remove when minimum PXR_VERSION >= 2405
    (collections)
    (filterLink)
);

TF_REGISTRY_FUNCTION(TfType)
{
    HdSceneIndexPluginRegistry::Define<HdPrman_DependencySceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    // This scene index should be added *before*
    // HdPrman_DependencyForwardingSceneIndexPlugin (which currently uses 1000).
    const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase
        = 100;

    for(const auto& rendererDisplayName : HdPrman_GetPluginDisplayNames()) {
        HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
            rendererDisplayName,
            _tokens->sceneIndexPluginName,
            nullptr,
            insertionPhase,
            HdSceneIndexPluginRegistry::InsertionOrderAtStart);
    }
}

namespace
{

/// Given a prim path data source, returns a dependency of volumeFieldBinding
/// on volumeField of that given prim.
/// So, if the volume field's file path was modified, the volumeFieldBinding
/// would be invalidated to have the volume prim pick up the new asset.
/// Note: HdPrman_Field currently invalidates *all* Rprims backed by the render
///       index if DirtyParams is set on the field. We should be able to remove
///       that workaround with this scene index plugin in play.
///
HdDataSourceBaseHandle
_ComputeVolumeFieldDependency(const HdDataSourceBaseHandle &src)
{
    HdDependencySchema::Builder builder;

    builder.SetDependedOnPrimPath(HdPathDataSource::Cast(src));

    static HdLocatorDataSourceHandle dependedOnLocatorDataSource =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdVolumeFieldSchema::GetDefaultLocator());
    builder.SetDependedOnDataSourceLocator(dependedOnLocatorDataSource);

    static HdLocatorDataSourceHandle affectedLocatorDataSource =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdVolumeFieldBindingSchema::GetDefaultLocator());
    builder.SetAffectedDataSourceLocator(affectedLocatorDataSource);
    return builder.Build();
}

/// Given a prim path, returns a dependency of __dependencies
/// on volumeFieldBinding of the given prim.

HdContainerDataSourceHandle
_ComputeVolumeFieldBindingDependency(const SdfPath &primPath)
{
    HdDependencySchema::Builder builder;

    builder.SetDependedOnPrimPath(
        HdRetainedTypedSampledDataSource<SdfPath>::New(
            primPath));

    static HdLocatorDataSourceHandle dependedOnLocatorDataSource =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdVolumeFieldBindingSchema::GetDefaultLocator());
    builder.SetDependedOnDataSourceLocator(dependedOnLocatorDataSource);

    static HdLocatorDataSourceHandle affectedLocatorDataSource =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdDependenciesSchema::GetDefaultLocator());
    builder.SetAffectedDataSourceLocator(affectedLocatorDataSource);

    return
        HdRetainedContainerDataSource::New(
            HdVolumeFieldBindingSchemaTokens->volumeFieldBinding,
            builder.Build());
}

HdContainerDataSourceHandle
_ComputeVolumeFieldBindingDependencies(
    const SdfPath &primPath,
    const HdContainerDataSourceHandle &primSource)
{
    return HdOverlayContainerDataSource::New(
        HdMapContainerDataSource::New(
            _ComputeVolumeFieldDependency,
            HdContainerDataSource::Cast(
                HdContainerDataSource::Get(
                    primSource,
                    HdVolumeFieldBindingSchema::GetDefaultLocator()))),
        _ComputeVolumeFieldBindingDependency(primPath));
}

HdContainerDataSourceHandle
_BuildLightFilterDependenciesDs(const SdfPathVector &filterPaths)
{
    if (filterPaths.empty()) {
        return nullptr;
    }

    TfTokenVector names;
    std::vector<HdDataSourceBaseHandle> deps;
    const size_t numFilters = filterPaths.size();

    // For each filter targeted by the light, register 2 dependencies to
    // forward light filter linking and visibility invalidation to the light.
    const size_t numDeps =
        1 /* __dependencies -> filters */ +  2 * numFilters;
    names.reserve(numDeps);
    deps.reserve(numDeps);

    static HdLocatorDataSourceHandle filtersLocDs =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdLightSchema::GetDefaultLocator().Append(HdTokens->filters));

    static HdLocatorDataSourceHandle dependenciesLocDs =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdDependenciesSchema::GetDefaultLocator());

    names.push_back(_tokens->__dependenciesToFilters);
    deps.push_back(
        HdDependencySchema::Builder()
            .SetDependedOnPrimPath(/* self */ nullptr)
            .SetDependedOnDataSourceLocator(filtersLocDs)
            .SetAffectedDataSourceLocator(dependenciesLocDs)
            .Build());

    static HdLocatorDataSourceHandle filterLinkLocDs =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
#if PXR_VERSION >= 2405
            HdCollectionsSchema::GetDefaultLocator()
                .Append(HdTokens->filterLink));
#else
            HdDataSourceLocator(_tokens->collections)
                .Append(_tokens->filterLink));
#endif

    static HdLocatorDataSourceHandle filterVisLocDs =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            HdVisibilitySchema::GetDefaultLocator());

    static HdLocatorDataSourceHandle affectedLocatorDs =
        HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
            // XXX This should be more targeted.
            HdLightSchema::GetDefaultLocator());

    for (const auto &filterPath : filterPaths) {
        names.push_back(TfToken(filterPath.GetAsString() + "_linkDep"));
        deps.push_back(
            HdDependencySchema::Builder()
            .SetDependedOnPrimPath(
                HdRetainedTypedSampledDataSource<SdfPath>::New(filterPath))
            .SetDependedOnDataSourceLocator(filterLinkLocDs)
            .SetAffectedDataSourceLocator(affectedLocatorDs)
            .Build());

        names.push_back(TfToken(filterPath.GetAsString() + "_visDep"));
        deps.push_back(
            HdDependencySchema::Builder()
            .SetDependedOnPrimPath(
                HdRetainedTypedSampledDataSource<SdfPath>::New(filterPath))
            .SetDependedOnDataSourceLocator(filterVisLocDs)
            .SetAffectedDataSourceLocator(affectedLocatorDs)
            .Build());
    }

    return HdRetainedContainerDataSource::New(
        numDeps, names.data(), deps.data());
}

HdContainerDataSourceHandle
_ComputeLightFilterDependencies(
    const SdfPath &lightPrimPath,
    const HdContainerDataSourceHandle &lightPrimSource)
{
#if PXR_VERSION >= 2405
    const
#endif
    HdLightSchema ls = HdLightSchema::GetFromParent(lightPrimSource);

    // XXX
    // HdLightSchema is barebones at the moment, so we need to explicitly use
    // the 'filters' token below.
    const HdContainerDataSourceHandle lightDs = ls.GetContainer();
    if (lightDs) {
        HdDataSourceBaseHandle filtersDs = lightDs->Get(HdTokens->filters);
        if (HdSampledDataSourceHandle valDs =
                HdSampledDataSource::Cast(filtersDs)) {

            VtValue val = valDs->GetValue(0.0f);
            if (val.IsHolding<SdfPathVector>()) {
                return _BuildLightFilterDependenciesDs(
                    val.UncheckedGet<SdfPathVector>());
            }
        }
    }

    return nullptr;
}

TF_DECLARE_REF_PTRS(_SceneIndex);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/// \class _SceneIndex
///
/// The scene index that adds dependencies for volume and light prims.
///
class _SceneIndex : public HdSingleInputFilteringSceneIndexBase
{
public:
    static _SceneIndexRefPtr New(
        const HdSceneIndexBaseRefPtr &inputSceneIndex)
    {
        return TfCreateRefPtr(new _SceneIndex(inputSceneIndex));
    }

    HdSceneIndexPrim GetPrim(const SdfPath &primPath) const override
    {
        const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
        if (prim.primType == HdPrimTypeTokens->volume) {
            return
                { prim.primType,
                  HdContainerDataSourceEditor(prim.dataSource)
                      .Overlay(
                          HdDependenciesSchema::GetDefaultLocator(),
                          HdLazyContainerDataSource::New(
                              std::bind(_ComputeVolumeFieldBindingDependencies,
                                        primPath, prim.dataSource)))
                      .Finish() };
        }

        if (HdPrimTypeIsLight(prim.primType)) {
            return
                { prim.primType,
                  HdContainerDataSourceEditor(prim.dataSource)
                      .Overlay(
                          HdDependenciesSchema::GetDefaultLocator(),
                          HdLazyContainerDataSource::New(
                              std::bind(_ComputeLightFilterDependencies,
                                        primPath, prim.dataSource)))
                      .Finish() };
        }
        return prim;
    }

    SdfPathVector GetChildPrimPaths(const SdfPath &primPath) const override
    {
        return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

protected:
    _SceneIndex(
        const HdSceneIndexBaseRefPtr &inputSceneIndex)
      : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    {
#if PXR_VERSION >= 2308
        SetDisplayName("HdPrman: declare dependencies");
#endif
    }

    void _PrimsAdded(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::AddedPrimEntries &entries) override
    {
        if (!_IsObserved()) {
            return;
        }

        _SendPrimsAdded(entries);
    }

    void _PrimsRemoved(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::RemovedPrimEntries &entries) override
    {
        if (!_IsObserved()) {
            return;
        }

        _SendPrimsRemoved(entries);
    }

    void _PrimsDirtied(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::DirtiedPrimEntries &entries) override
    {
        HD_TRACE_FUNCTION();

        if (!_IsObserved()) {
            return;
        }

        _SendPrimsDirtied(entries);
    }
};

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Implementation of HdPrman_DependencySceneIndexPlugin

HdPrman_DependencySceneIndexPlugin::HdPrman_DependencySceneIndexPlugin() = default;

HdSceneIndexBaseRefPtr
HdPrman_DependencySceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene,
    const HdContainerDataSourceHandle &inputArgs)
{
    return _SceneIndex::New(inputScene);
}

PXR_NAMESPACE_CLOSE_SCOPE
