#include "FastDfsClient.h"

#include <cstring>
#include <cstdlib>

#include <fastdfs/fdfs_client.h>
#include <fastcommon/logger.h>

#include "Logger.h"

namespace fileagent
{

    FastDfsClient::~FastDfsClient()
    {
        if (initialized_)
        {
            fdfs_client_destroy();
        }
    }

    bool FastDfsClient::init(const std::string &conf_file)
    {
        if (initialized_)
            return true;

        log_init();
        // FastDFS 日志级别设为 ERR，避免输出过多调试信息
        g_log_context.log_level = LOG_ERR;
        ignore_signal_pipe();

        if (fdfs_client_init(conf_file.c_str()) != 0)
        {
            last_error_ = "fdfs_client_init failed";
            logError("FastDfsClient: " + last_error_ + " from " + conf_file);
            return false;
        }

        initialized_ = true;
        logInfo("FastDfsClient: initialized from " + conf_file);
        return true;
    }

    std::string FastDfsClient::getFileExtension(const std::string &filename)
    {
        size_t dot = filename.find_last_of('.');
        if (dot == std::string::npos || dot + 1 >= filename.size())
            return "";
        return filename.substr(dot + 1);
    }

    bool FastDfsClient::upload(const std::string &content,
                               const std::string &file_ext,
                               std::string &file_id)
    {
        if (!initialized_)
        {
            last_error_ = "FastDfsClient not initialized";
            return false;
        }

        ConnectionInfo *pTrackerServer = tracker_get_connection();
        if (pTrackerServer == NULL)
        {
            last_error_ = "failed to connect to tracker";
            logError("FastDfsClient::upload: " + last_error_);
            return false;
        }

        ConnectionInfo storageServer = {0};
        char group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
        int store_path_index = 0;
        int result = tracker_query_storage_store_without_group(
            pTrackerServer, &storageServer, group_name, &store_path_index);
        if (result != 0)
        {
            last_error_ = "tracker_query_storage_store failed: " + std::string(STRERROR(result));
            logError("FastDfsClient::upload: " + last_error_);
            tracker_close_connection_ex(pTrackerServer, true);
            return false;
        }

        char fid[128] = {0};
        const char *ext = file_ext.empty() ? NULL : file_ext.c_str();
        result = storage_upload_by_filebuff1(
            pTrackerServer, &storageServer,
            store_path_index,
            content.data(), static_cast<int64_t>(content.size()),
            ext,       // file_ext_name
            NULL,      // meta_list
            0,         // meta_count
            NULL,      // group_name (auto)
            fid);      // file_id output

        tracker_close_connection_ex(pTrackerServer, true);

        if (result != 0)
        {
            last_error_ = "storage_upload_by_filebuff1 failed: " + std::string(STRERROR(result));
            logError("FastDfsClient::upload: " + last_error_);
            return false;
        }

        file_id = fid;
        logDebug("FastDfsClient::upload: " + file_id + " (" +
                 std::to_string(content.size()) + " bytes)");
        return true;
    }

    bool FastDfsClient::upload(const std::string &content,
                               const std::string &filename,
                               std::string &file_id,
                               bool /*unused*/)
    {
        return upload(content, getFileExtension(filename), file_id);
    }

    bool FastDfsClient::download(const std::string &fid,
                                 std::string &content)
    {
        if (!initialized_)
        {
            last_error_ = "FastDfsClient not initialized";
            return false;
        }

        ConnectionInfo *pTrackerServer = tracker_get_connection();
        if (pTrackerServer == NULL)
        {
            last_error_ = "failed to connect to tracker";
            logError("FastDfsClient::download: " + last_error_);
            return false;
        }

        char *file_buff = NULL;
        int64_t file_size = 0;

        int result = storage_download_file_to_buff1(
            pTrackerServer, NULL, fid.c_str(),
            &file_buff, &file_size);

        tracker_close_connection_ex(pTrackerServer, true);

        if (result != 0)
        {
            last_error_ = "download failed: " + std::string(STRERROR(result));
            logError("FastDfsClient::download: " + last_error_);
            if (file_buff)
                free(file_buff);
            return false;
        }

        content.assign(file_buff, static_cast<size_t>(file_size));
        free(file_buff);

        logDebug("FastDfsClient::download: " + fid +
                 " (" + std::to_string(file_size) + " bytes)");
        return true;
    }

    bool FastDfsClient::remove(const std::string &fid)
    {
        if (!initialized_)
        {
            last_error_ = "FastDfsClient not initialized";
            return false;
        }

        ConnectionInfo *pTrackerServer = tracker_get_connection();
        if (pTrackerServer == NULL)
        {
            last_error_ = "failed to connect to tracker";
            logError("FastDfsClient::remove: " + last_error_);
            return false;
        }

        int result = storage_delete_file1(pTrackerServer, NULL, fid.c_str());

        tracker_close_connection_ex(pTrackerServer, true);

        if (result != 0)
        {
            last_error_ = "delete failed: " + std::string(STRERROR(result));
            logError("FastDfsClient::remove: " + last_error_);
            return false;
        }

        logDebug("FastDfsClient::remove: " + fid);
        return true;
    }

} // namespace fileagent
