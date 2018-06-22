//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/pxr.h"
#include "pxrUsd/api.h"

#include "pxrUsdMayaGL/proxyDrawOverride.h"
#include "pxrUsdMayaGL/proxyShapeUI.h"

#include "usdMaya/diagnosticDelegate.h"
#include "usdMaya/proxyShape.h"
#include "usdMaya/referenceAssembly.h"
#include "usdMaya/stageData.h"
#include "usdMaya/undoHelperCmd.h"
#include "usdMaya/usdImport.h"
#include "usdMaya/usdExport.h"
#include "usdMaya/usdListShadingModes.h"
#include "usdMaya/usdTranslatorImport.h"
#include "usdMaya/usdTranslatorExport.h"

#include <maya/MDrawRegistry.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MPxNode.h>
#include <maya/MStatus.h>


PXR_NAMESPACE_USING_DIRECTIVE


PXRUSD_API
MStatus
initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj, "Pixar", "1.0", "Any");

    status = plugin.registerData(
        UsdMayaStageData::typeName,
        UsdMayaStageData::mayaTypeId,
        UsdMayaStageData::creator);
    CHECK_MSTATUS(status);

    status = plugin.registerShape(
        UsdMayaProxyShape::typeName,
        UsdMayaProxyShape::typeId,
        UsdMayaProxyShape::creator,
        UsdMayaProxyShape::initialize,
        UsdMayaProxyShapeUI::creator,
        &UsdMayaProxyDrawOverride::sm_drawDbClassification);
    CHECK_MSTATUS(status);

    status = plugin.registerNode(
        UsdMayaReferenceAssembly::typeName,
        UsdMayaReferenceAssembly::typeId,
        UsdMayaReferenceAssembly::creator,
        UsdMayaReferenceAssembly::initialize,
        MPxNode::kAssembly,
        &UsdMayaReferenceAssembly::_classification);
    CHECK_MSTATUS(status);

    status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
        UsdMayaProxyDrawOverride::sm_drawDbClassification,
        UsdMayaProxyDrawOverride::sm_drawRegistrantId,
        UsdMayaProxyDrawOverride::Creator);
    CHECK_MSTATUS(status);

    status = MGlobal::sourceFile("usdMaya.mel");
    CHECK_MSTATUS(status);

    // Set the label for the assembly node type so that it appears correctly
    // in the 'Create -> Scene Assembly' menu.
    const MString assemblyTypeLabel("UsdReferenceAssembly");
    MString setLabelCmd;
    status = setLabelCmd.format("assembly -e -type ^1s -label ^2s",
                                UsdMayaReferenceAssembly::typeName,
                                assemblyTypeLabel);
    CHECK_MSTATUS(status);
    status = MGlobal::executeCommand(setLabelCmd);
    CHECK_MSTATUS(status);

    // Procs stored in usdMaya.mel
    // Add assembly callbacks for accessing data without creating an MPxAssembly instance
    status = MGlobal::executeCommand("assembly -e -repTypeLabelProc usdMaya_UsdMayaReferenceAssembly_repTypeLabel -type " + UsdMayaReferenceAssembly::typeName);
    CHECK_MSTATUS(status);
    status = MGlobal::executeCommand("assembly -e -listRepTypesProc usdMaya_UsdMayaReferenceAssembly_listRepTypes -type " + UsdMayaReferenceAssembly::typeName);
    CHECK_MSTATUS(status);

    // Attribute Editor Templates
    // XXX: The try/except here is temporary until we change the Pixar-internal
    // package name to match the external package name.
    MString attribEditorCmd(
        "try:\n"
        "    from pxr.UsdMaya import AEpxrUsdReferenceAssemblyTemplate\n"
        "except ImportError:\n"
        "    from pixar.UsdMaya import AEpxrUsdReferenceAssemblyTemplate\n"
        "AEpxrUsdReferenceAssemblyTemplate.addMelFunctionStubs()");
    status = MGlobal::executePythonCommand(attribEditorCmd);
    CHECK_MSTATUS(status);

    status = plugin.registerCommand(
        "usdExport",
        usdExport::creator,
        usdExport::createSyntax);
    if (!status) {
        status.perror("registerCommand usdExport");
    }

    status = plugin.registerCommand(
        "usdImport",
        usdImport::creator,
        usdImport::createSyntax);
    if (!status) {
        status.perror("registerCommand usdImport");
    }

    status = plugin.registerCommand(
        "usdListShadingModes",
        usdListShadingModes::creator,
        usdListShadingModes::createSyntax);
    if (!status) {
        status.perror("registerCommand usdListShadingModes");
    }

    status = plugin.registerCommand(
        "usdUndoHelperCmd",
        PxrUsdMayaUndoHelperCmd::creator,
        PxrUsdMayaUndoHelperCmd::createSyntax);
    if (!status) {
        status.perror("registerCommand usdUndoHelperCmd");
    }

    status = plugin.registerFileTranslator(
        "pxrUsdImport",
        "",
        usdTranslatorImport::creator,
        "usdTranslatorImport", // options script name
        const_cast<char*>(usdTranslatorImport::GetDefaultOptions().c_str()),
        false);
    if (!status) {
        status.perror("pxrUsd: unable to register USD Import translator.");
    }

    status = plugin.registerFileTranslator(
        "pxrUsdExport",
        "",
        usdTranslatorExport::creator,
        "usdTranslatorExport", // options script name
        const_cast<char*>(usdTranslatorExport::GetDefaultOptions().c_str()),
        true);
    if (!status) {
        status.perror("pxrUsd: unable to register USD Export translator.");
    }

    PxrUsdMayaDiagnosticDelegate::InstallDelegate();

    return status;
}

PXRUSD_API
MStatus
uninitializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj);

    status = plugin.deregisterCommand("usdImport");
    if (!status) {
        status.perror("deregisterCommand usdImport");
    }

    status = plugin.deregisterCommand("usdExport");
    if (!status) {
        status.perror("deregisterCommand usdExport");
    }

    status = plugin.deregisterCommand("usdListShadingModes");
    if (!status) {
        status.perror("deregisterCommand usdListShadingModes");
    }

    status = plugin.deregisterCommand("usdUndoHelperCmd");
    if (!status) {
        status.perror("deregisterCommand usdUndoHelperCmd");
    }

    status = plugin.deregisterFileTranslator("pxrUsdImport");
    if (!status) {
        status.perror("pxrUsd: unable to deregister USD Import translator.");
    }

    status = plugin.deregisterFileTranslator("pxrUsdExport");
    if (!status) {
        status.perror("pxrUsd: unable to deregister USD Export translator.");
    }

    status = MGlobal::executeCommand("assembly -e -deregister " + UsdMayaReferenceAssembly::typeName);
    CHECK_MSTATUS(status);

    status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
        UsdMayaProxyDrawOverride::sm_drawDbClassification,
        UsdMayaProxyDrawOverride::sm_drawRegistrantId);
    CHECK_MSTATUS(status);

    status = plugin.deregisterNode(UsdMayaReferenceAssembly::typeId);
    CHECK_MSTATUS(status);

    status = plugin.deregisterNode(UsdMayaProxyShape::typeId);
    CHECK_MSTATUS(status);

    status = plugin.deregisterData(UsdMayaStageData::mayaTypeId);
    CHECK_MSTATUS(status);

    PxrUsdMayaDiagnosticDelegate::RemoveDelegate();

    return status;
}