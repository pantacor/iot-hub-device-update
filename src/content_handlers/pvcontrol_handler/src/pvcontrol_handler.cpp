/**
 * @file pvcontrol_handler.cpp
 * @brief Implementation of ContentHandler API for pvcontrol.
 *
 * Will call into wrapper script for pvcontrol to install image files.
 *
 * microsoft/pvcontrol
 * v1:
 *   Description:
 *   Initial revision.
 *
 *   Expected files:
 *   .swu - contains pvcontrol image.
 *
 * @copyright Copyright (c) Microsoft Corporation.
 * @copyright Copyright (c) 2022, Pantacor Ltd.
 * Licensed under the MIT License.
 */
#include "aduc/pvcontrol_handler.hpp"

#include "aduc/adu_core_exports.h"
#include "aduc/extension_manager.hpp"
#include "aduc/logging.h"
#include "aduc/process_utils.hpp"
#include "aduc/string_c_utils.h"
#include "aduc/string_utils.hpp"
#include "aduc/system_utils.h"
#include "aduc/types/update_content.h"
#include "aduc/workflow_data_utils.h"
#include "aduc/workflow_utils.h"
#include "adushell_const.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include <dirent.h>

namespace adushconst = Adu::Shell::Const;

EXTERN_C_BEGIN
/**
 * @brief Instantiates an Update Content Handler for 'microsoft/pvcontrol:1' update type.
 */
ContentHandler* CreateUpdateContentHandlerExtension(ADUC_LOG_SEVERITY logLevel)
{
    ADUC_Logging_Init(logLevel, "pvcontrol-handler");
    Log_Info("Instantiating an Update Content Handler for 'microsoft/pvcontrol:1'");
    try
    {
        return PVControlHandlerImpl::CreateContentHandler();
    }
    catch (const std::exception& e)
    {
        const char* what = e.what();
        Log_Error("Unhandled std exception: %s", what);
    }
    catch (...)
    {
        Log_Error("Unhandled exception");
    }

    return nullptr;
}
EXTERN_C_END

/**
 * @brief Destructor for the PVControl Handler Impl class.
 */
PVControlHandlerImpl::~PVControlHandlerImpl() // override
{
    ADUC_Logging_Uninit();
}

// Forward declarations.
static ADUC_Result CancelApply(const char* logFolder);

/**
 * @brief Creates a new PVControlHandlerImpl object and casts to a ContentHandler.
 * Note that there is no way to create a PVControlHandlerImpl directly.
 *
 * @return ContentHandler* SimulatorHandlerImpl object as a ContentHandler.
 */
ContentHandler* PVControlHandlerImpl::CreateContentHandler()
{
    return new PVControlHandlerImpl();
}

/**
 * @brief Performs 'Download' task.
 *
 * @return ADUC_Result The result of the download (always success)
 */
ADUC_Result PVControlHandlerImpl::Download(const tagADUC_WorkflowData* workflowData)
{
    std::stringstream updateFilename;
    ADUC_Result result = { ADUC_Result_Failure };
    ADUC_FileEntity* entity = nullptr;
    ADUC_WorkflowHandle workflowHandle = workflowData->WorkflowHandle;
    char* workflowId = workflow_get_id(workflowHandle);
    char* workFolder = workflow_get_workfolder(workflowHandle);
    int fileCount = 0;

    char* updateType = workflow_get_update_type(workflowHandle);
    char* updateName = nullptr;
    unsigned int updateTypeVersion = 0;
    bool updateTypeOk = ADUC_ParseUpdateType(updateType, &updateName, &updateTypeVersion);

    if (!updateTypeOk)
    {
        Log_Error("PVControl packages download failed. Unknown Handler Version (UpdateDateType:%s)", updateType);
        result.ExtendedResultCode = ADUC_ERC_SWUPDATE_HANDLER_DOWNLOAD_FAILURE_UNKNOW_UPDATE_VERSION;
        goto done;
    }

    if (updateTypeVersion != 1)
    {
        Log_Error("PVControl packages download failed. Wrong Handler Version %d", updateTypeVersion);
        result.ExtendedResultCode = ADUC_ERC_SWUPDATE_HANDLER_DOWNLOAD_FAILURE_WRONG_UPDATE_VERSION;
        goto done;
    }

    // For 'microsoft/pvcontrol:1', we're expecting 1 payload file.
    fileCount = workflow_get_update_files_count(workflowHandle);
    if (fileCount != 1)
    {
        Log_Error("PVControl expecting one file. (%d)", fileCount);
        result.ExtendedResultCode = ADUC_ERC_SWUPDATE_HANDLER_DOWNLOAD_FAILURE_WRONG_FILECOUNT;
        goto done;
    }

    if (!workflow_get_update_file(workflowHandle, 0, &entity))
    {
        result.ExtendedResultCode = ADUC_ERC_SWUPDATE_HANDLER_DOWNLOADE_BAD_FILE_ENTITY;
        goto done;
    }

    updateFilename << workFolder << "/" << entity->TargetFilename;

    result = ExtensionManager::Download(entity, workflowId, workFolder, DO_RETRY_TIMEOUT_DEFAULT, nullptr);

done:
    workflow_free_string(workflowId);
    workflow_free_string(workFolder);
    workflow_free_file_entity(entity);

    return result;
}

/**
 * @brief Install implementation for pvcontrol.
 * Calls into the pvcontrol wrapper script to install an image file.
 *
 * @return ADUC_Result The result of the install.
 */
ADUC_Result PVControlHandlerImpl::Install(const tagADUC_WorkflowData* workflowData)
{
    ADUC_Result result = { ADUC_Result_Failure };
    ADUC_FileEntity* entity = nullptr;
    ADUC_WorkflowHandle workflowHandle = workflowData->WorkflowHandle;
    char* workFolder = workflow_get_workfolder(workflowData->WorkflowHandle);

    Log_Info("Installing from %s", workFolder);
    std::unique_ptr<DIR, std::function<int(DIR*)>> directory(
        opendir(workFolder), [](DIR* dirp) -> int { return closedir(dirp); });
    if (directory == nullptr)
    {
        Log_Error("opendir failed, errno = %d", errno);

        result = { .ResultCode = ADUC_Result_Failure,
                   .ExtendedResultCode = ADUC_ERC_SWUPDATE_HANDLER_INSTALL_FAILURE_CANNOT_OPEN_WORKFOLDER };
        goto done;
    }

    if (!workflow_get_update_file(workflowHandle, 0, &entity))
    {
        result.ExtendedResultCode = ADUC_ERC_SWUPDATE_HANDLER_INSTALL_FAILURE_BAD_FILE_ENTITY;
        goto done;
    }

	{
		std::string command = adushconst::adu_shell;
		std::vector<std::string> args{ adushconst::update_type_opt,
                                       adushconst::update_type_pantacor_pvcontrol,
                                       adushconst::update_action_opt,
                                       adushconst::update_action_install };

        std::stringstream data;
        data << workFolder << "/" << entity->TargetFilename;
        args.emplace_back(adushconst::target_data_opt);
        args.emplace_back(data.str().c_str());

        args.emplace_back(adushconst::target_log_folder_opt);
        args.emplace_back(ADUC_LOG_FOLDER);

        std::string output;
        const int exitCode = ADUC_LaunchChildProcess(command, args, output);

        if (exitCode != 0)
        {
            Log_Error("Install failed, extendedResultCode = %d", exitCode);
            result = { .ResultCode = ADUC_Result_Failure, .ExtendedResultCode = exitCode };
            goto done;
        }
	}

    Log_Info("Install succeeded");
    result.ResultCode = ADUC_Result_Install_Success;

done:
    workflow_free_string(workFolder);
    workflow_free_file_entity(entity);
    return result;
}

/**
 * @brief Apply implementation for pvcontrol.
 * Calls into the pvcontrol wrapper script to perform apply.
 * Will flip bootloader flag to boot into update partition for A/B update.
 *
 * @return ADUC_Result The result of the apply.
 */
ADUC_Result PVControlHandlerImpl::Apply(const tagADUC_WorkflowData* workflowData)
{
    ADUC_Result result = { ADUC_Result_Failure };

    Log_Info("Applying data from %s", ADUC_PVINSTALLED_FILE_PATH);

    JSON_Value* progressData = json_parse_file(ADUC_PVINSTALLED_FILE_PATH);
    if (progressData != nullptr)
    {
		Log_Error("Could not load pvinstalled file");
        result = { ADUC_Result_Failure };
		return result;
	}

    const std::string revision = json_object_get_string(json_object(progressData), "revision");

    Log_Info("Applying revision %s", revision);

    {
        std::string command = adushconst::adu_shell;
        std::vector<std::string> args{ adushconst::update_type_opt,
                                       adushconst::update_type_pantacor_pvcontrol,
                                       adushconst::update_action_opt,
                                       adushconst::update_action_apply };

        args.emplace_back(adushconst::target_data_opt);
        args.emplace_back(revision);

        args.emplace_back(adushconst::target_log_folder_opt);
        args.emplace_back(ADUC_LOG_FOLDER);

        std::string output;
        const int exitCode = ADUC_LaunchChildProcess(command, args, output);

        if (exitCode != 0)
        {
            Log_Error("Apply failed, extendedResultCode = %d", exitCode);
            result = { ADUC_Result_Failure, exitCode };
			goto done;
        }

        Log_Info("Apply succeeded");
		result = { ADUC_Result_Apply_Success };
    }

done:
	return result;
}

/**
 * @brief Cancel implementation for pvcontrol.
 * We don't have many hooks into pvcontrol to cancel an ongoing install.
 * We can cancel apply by reverting the bootloader flag to boot into the original partition.
 * Calls into the pvcontrol wrapper script to cancel apply.
 * Cancel after or during any other operation is a no-op.
 *
 * @return ADUC_Result The result of the cancel.
 */
ADUC_Result PVControlHandlerImpl::Cancel(const tagADUC_WorkflowData* workflowData)
{
    ADUC_Result result = { ADUC_Result_Failure };

	Log_Info("Cancel not implemented");

	result = { ADUC_Result_Cancel_Success };
    return result;
}

/**
 * @brief Checks if the installed content matches the installed criteria.
 *
 * @param installedCriteria The installed criteria string. e.g. The firmware version.
 *  installedCriteria has already been checked to be non-empty before this call.
 *
 * @return ADUC_Result
 */
ADUC_Result PVControlHandlerImpl::IsInstalled(const tagADUC_WorkflowData* workflowData)
{
	ADUC_Result result = { ADUC_Result_IsInstalled_NotInstalled };
    char* installedCriteria = ADUC_WorkflowData_GetInstalledCriteria(workflowData);

    Log_Info("Getting status from revision %s", installedCriteria);

	{
        std::string command = adushconst::adu_shell;
        std::vector<std::string> args{ adushconst::update_type_opt,
                                       adushconst::update_type_pantacor_pvcontrol,
                                       adushconst::update_action_opt,
                                       adushconst::update_action_get_status };

        args.emplace_back(adushconst::target_data_opt);
        args.emplace_back(installedCriteria);

        std::string output;
        const int exitCode = ADUC_LaunchChildProcess(command, args, output);

        if (exitCode != 0)
        {
            Log_Error("Get status failed, extendedResultCode = %d", exitCode);
        }
    }

    Log_Info("Checking revision %s status from %s",
		installedCriteria,
		ADUC_PVPROGRESS_FILE_PATH);

    JSON_Value* progressData = json_parse_file(ADUC_PVPROGRESS_FILE_PATH);
    if (progressData != nullptr)
    {
		Log_Error("Could not load pvprogress file");
        workflow_free_string(installedCriteria);
		return result;
	}

    const std::string status = json_object_get_string(json_object(progressData), "status");
    if ((status == "DONE") || (status == "UPDATED"))
    {
        Log_Info("Update succeded with status %s", status);
        result = { ADUC_Result_IsInstalled_Installed };
    } else if ((status == "ERROR") || (status == "WONTGO"))
    {
        Log_Error("Update failed with status %s", status);
    }
    Log_Info("Update still in progress");

done:
    workflow_free_string(installedCriteria);
	return result;
}

/**
 * @brief Helper function to perform cancel when we are doing an apply.
 *
 * @return ADUC_Result The result of the cancel.
 */
static ADUC_Result CancelApply(const char* logFolder)
{
    ADUC_Result result = { ADUC_Result_Failure };

	Log_Info("CancelApply not implemented");

	result = { ADUC_Result_Cancel_Success };
    return result;
}
