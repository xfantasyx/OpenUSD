//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/usdValidation/usdUtilsValidators/validatorTokens.h"
#include "pxr/usdValidation/usdValidation/error.h"
#include "pxr/usdValidation/usdValidation/registry.h"
#include "pxr/usdValidation/usdValidation/validator.h"

#include <array>
#include <filesystem>

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_PRIVATE_TOKENS(_tokens,
    ((usdUtilsValidatorsPlugin, "usdUtilsValidators"))
);

static void
TestUsdUsdzValidators()
{
    // This should be updated with every new validator added with
    // UsdUtilsValidators keyword.
    UsdValidationRegistry &registry = UsdValidationRegistry::GetInstance();
    UsdValidationValidatorMetadataVector metadata
        = registry.GetValidatorMetadataForPlugin(
            _tokens->usdUtilsValidatorsPlugin);
    TF_AXIOM(metadata.size() == 4);
    // Since other validators can be registered with a UsdUtilsValidators
    // keyword, our validators registered in usd are a subset of the entire
    // set.
    std::set<TfToken> validatorMetadataNameSet;
    for (const UsdValidationValidatorMetadata &metadata : metadata) {
        validatorMetadataNameSet.insert(metadata.name);
    }

    const std::set<TfToken> expectedValidatorNames
        = { UsdUtilsValidatorNameTokens->packageEncapsulationValidator,
            UsdUtilsValidatorNameTokens->fileExtensionValidator,
            UsdUtilsValidatorNameTokens->rootPackageValidator,
            UsdUtilsValidatorNameTokens->usdzPackageValidator };

    TF_AXIOM(validatorMetadataNameSet == expectedValidatorNames);
}


void ValidateError(const UsdValidationError &error,
        const std::string& expectedErrorMsg,
        const TfToken& expectedErrorIdentifier,
        UsdValidationErrorType expectedErrorType =
        UsdValidationErrorType::Error)
{
    TF_AXIOM(error.GetIdentifier() == expectedErrorIdentifier);
    TF_AXIOM(error.GetType() == expectedErrorType);
    TF_AXIOM(error.GetSites().size() == 1u);
    const UsdValidationErrorSite &errorSite = error.GetSites()[0];
    TF_AXIOM(!errorSite.GetLayer().IsInvalid());
    TF_AXIOM(error.GetMessage() == expectedErrorMsg);
}

static void
ValidateError(const UsdValidationError &error,
    const std::string &expectedErrorMessage,
    const TfToken &expectedIdentifier,
    const UsdValidationErrorType &expectedErrorType =
    UsdValidationErrorType::Error)
{
    TF_AXIOM(error.GetMessage() == expectedErrorMessage);
    auto errorIdentifier = error.GetIdentifier().GetText();
    TF_AXIOM(error.GetIdentifier() == expectedIdentifier);
    TF_AXIOM(error.GetType() == expectedErrorType);
    TF_AXIOM(error.GetSites().size() == 1);
    TF_AXIOM(!error.GetSites()[0].GetLayer().IsInvalid());
}

static void
TestPackageEncapsulationValidator()
{
    UsdValidationRegistry &registry = UsdValidationRegistry::GetInstance();

    // Verify the validator exists
    const UsdValidationValidator *validator = registry.GetOrLoadValidatorByName(
        UsdUtilsValidatorNameTokens->packageEncapsulationValidator);

    TF_AXIOM(validator);

    // Load the pre-created usdz stage with paths to a layer and asset
    // that are not included in the package, but exist
    const UsdStageRefPtr &stage = UsdStage::Open("fail.usdz");

    {
        const UsdValidationErrorVector errors = validator->Validate(stage);

        // Verify both the layer & asset errors are present
        TF_AXIOM(errors.size() == 2u);

        // Note that we keep the referenced layer in normalized path to represent
        // the layer identifier, whereas the asset path is platform specific path,
        // as returned by UsdUtilsComputeAllDependencies
        const SdfLayerRefPtr &rootLayer = stage->GetRootLayer();
        const std::string &rootLayerIdentifier = rootLayer->GetIdentifier();
        const std::string realUsdzPath = rootLayer->GetRealPath();
        const std::string errorLayer
            = TfStringCatPaths(TfGetPathName(TfAbsPath(rootLayerIdentifier)),
                               "excludedDirectory/layer.usda");

        std::filesystem::path parentDir
            = std::filesystem::path(realUsdzPath).parent_path();
        const std::string errorAsset
            = (parentDir / "excludedDirectory" / "image.jpg").string();

        std::array<std::string, 2> expectedErrorMessages
            = { TfStringPrintf(("Found referenced layer '%s' that does not "
                                "belong to the package '%s'."),
                               errorLayer.c_str(), realUsdzPath.c_str()),
                TfStringPrintf(("Found asset reference '%s' that does not belong"
                                " to the package '%s'."),
                               errorAsset.c_str(), realUsdzPath.c_str()) };

        std::array<TfToken, 2> expectedErrorIdentifiers = {
            TfToken(
                "usdUtilsValidators:PackageEncapsulationValidator.LayerNotInPackage"),
            TfToken(
                "usdUtilsValidators:PackageEncapsulationValidator.AssetNotInPackage")
        };

        for (size_t i = 0; i < errors.size(); ++i) {
            ValidateError(errors[i], expectedErrorMessages[i],
                expectedErrorIdentifiers[i], UsdValidationErrorType::Warn);
        }
    }

    // Load the pre-created usdz stage with relative paths to both a reference
    // and an asset that are included in the package.
    const UsdStageRefPtr &passStage = UsdStage::Open("pass.usdz");
    {
        const UsdValidationErrorVector errors = validator->Validate(passStage);

        // Verify the errors are gone
        TF_AXIOM(errors.empty());
    }
}

static void
TestFileExtensionValidator()
{
    UsdValidationRegistry& registry = UsdValidationRegistry::GetInstance();

    // Verify the validator exists
    const UsdValidationValidator *validator = registry.GetOrLoadValidatorByName(
            UsdUtilsValidatorNameTokens->fileExtensionValidator);

    TF_AXIOM(validator);

    const UsdStageRefPtr& stage = UsdStage::Open("failFileExtension.usdz");
    UsdValidationErrorVector errors = validator->Validate(stage);
    TF_AXIOM(errors.size() == 1u);
    const std::string expectedErrorMsg =
        "File 'cube.invalid' in package 'failFileExtension.usdz' has an "
        "unknown unsupported extension 'invalid'.";
    const TfToken expectedErrorIdentifier(
    "usdUtilsValidators:FileExtensionValidator.UnsupportedFileExtensionInPackage");
    // Verify error occurs.
    ValidateError(errors[0], expectedErrorMsg, expectedErrorIdentifier);

    // Load a passing stage
    const UsdStageRefPtr& passStage = UsdStage::Open("allSupportedFiles.usdz");

    errors = validator->Validate(passStage);

    // Verify no errors occur with all valid extensions included
    TF_AXIOM(errors.empty());
}

static void
TestRootPackageValidator()
{
    UsdValidationRegistry &registry = UsdValidationRegistry::GetInstance();

    // Verify the validator exists
    const UsdValidationValidator *validator = registry.GetOrLoadValidatorByName(
        UsdUtilsValidatorNameTokens->rootPackageValidator);

    TF_AXIOM(validator);

    // Load the pre-created usdz stage with a compressed file in the root layer
    {
        const UsdStageRefPtr &stage =
            UsdStage::Open("packageWithCompressionAndByteAlignmentErrors.usdz");
        const UsdValidationErrorVector errors = validator->Validate(stage);

        // Verify compression error occurs
        TF_AXIOM(errors.size() == 3);

        const std::vector<std::string> expectedErrorMessages = {
            "File 'test.usda' in package "
            "'packageWithCompressionAndByteAlignmentErrors.usdz' has an "
            "invalid offset 39.",
            "File 'texture.jpg' in package "
            "'packageWithCompressionAndByteAlignmentErrors.usdz' has "
            "compression. Compression method is '8', actual size is 14. "
            "Uncompressed size is 1028.",
            "File 'texture.jpg' in package "
            "'packageWithCompressionAndByteAlignmentErrors.usdz' has an "
            "invalid offset 131."};
        const std::vector<TfToken> expectedErrorTokens = {
            TfToken("usdUtilsValidators:RootPackageValidator.ByteMisalignment"),
            TfToken("usdUtilsValidators:RootPackageValidator.CompressionDetected"),
            TfToken("usdUtilsValidators:RootPackageValidator.ByteMisalignment")};

        for (size_t i = 0; i < errors.size(); ++i)
        {
            ValidateError(errors[i], expectedErrorMessages[i],
                expectedErrorTokens[i]);
        }
    }

    // Load the pre-created usdz stage with valid files
    {
        const UsdStageRefPtr &stage =
            UsdStage::Open("pass.usdz");
        const UsdValidationErrorVector errors = validator->Validate(stage);

        // Verify there are no errors
        TF_AXIOM(errors.empty());
    }
}

int
main()
{
    TestUsdUsdzValidators();
    TestPackageEncapsulationValidator();
    TestFileExtensionValidator();
    TestRootPackageValidator();

    return EXIT_SUCCESS;
}
