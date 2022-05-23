/**
 * @file pvcontrol_handler.hpp
 * @brief Defines PVControlHandlerImpl.
 *
 * @copyright Copyright (c) Microsoft Corporation.
 * @copyright Copyright (c) 2022, Pantacor Ltd.
 * Licensed under the MIT License.
 */
#ifndef ADUC_PVCONTROL_HANDLER_HPP
#define ADUC_PVCONTROL_HANDLER_HPP

#include "aduc/content_handler.hpp"
#include "aduc/logging.h"
#include <aduc/result.h>
#include <memory>
#include <string>

EXTERN_C_BEGIN

/**
 * @brief Instantiates an Update Content Handler for 'microsoft/pvcontrol:1' update type.
 * @return A pointer to an instantiated Update Content Handler object.
 */
ContentHandler* CreateUpdateContentHandlerExtension(ADUC_LOG_SEVERITY logLevel);

EXTERN_C_END

/**
 * @class PVControlHandlerImpl
 * @brief The pvcontrol specific implementation of ContentHandler interface.
 */
class PVControlHandlerImpl : public ContentHandler
{
public:
    static ContentHandler* CreateContentHandler();

    // Delete copy ctor, copy assignment, move ctor and move assignment operators.
    PVControlHandlerImpl(const PVControlHandlerImpl&) = delete;
    PVControlHandlerImpl& operator=(const PVControlHandlerImpl&) = delete;
    PVControlHandlerImpl(PVControlHandlerImpl&&) = delete;
    PVControlHandlerImpl& operator=(PVControlHandlerImpl&&) = delete;

    ~PVControlHandlerImpl() override;

    ADUC_Result Download(const tagADUC_WorkflowData* workflowData) override;
    ADUC_Result Install(const tagADUC_WorkflowData* workflowData) override;
    ADUC_Result Apply(const tagADUC_WorkflowData* workflowData) override;
    ADUC_Result Cancel(const tagADUC_WorkflowData* workflowData) override;
    ADUC_Result IsInstalled(const tagADUC_WorkflowData* workflowData) override;

    static std::string ReadValueFromFile(const std::string& filePath);

protected:
    // Protected constructor, must call CreateContentHandler factory method or from derived simulator class
    PVControlHandlerImpl()
    {
    }
};

#endif // ADUC_PVCONTROL_HANDLER_HPP
