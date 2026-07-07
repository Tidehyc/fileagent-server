#pragma once

#include <string>

namespace fileagent
{

    /**
     * @brief FastDFS 客户端 C++ 封装
     *
     * 封装 FastDFS C API（libfdfsclient），提供上传/下载/删除操作。
     * 线程安全：每个操作独立获取 tracker 连接。
     */
    class FastDfsClient
    {
    public:
        FastDfsClient() = default;
        ~FastDfsClient();

        FastDfsClient(const FastDfsClient &) = delete;
        FastDfsClient &operator=(const FastDfsClient &) = delete;

        /**
         * @brief 从配置文件中初始化
         * @param conf_file FastDFS client.conf 路径
         * @return 初始化是否成功
         */
        bool init(const std::string &conf_file);

        /**
         * @brief 是否已初始化
         */
        bool initialized() const { return initialized_; }

        /**
         * @brief 上传文件内容（内存缓冲区）
         * @param content  文件内容
         * @param file_ext 文件扩展名（不含点号，可空）
         * @param file_id  输出：FastDFS file_id（如 group1/M00/00/00/xxx）
         * @return 是否成功
         */
        bool upload(const std::string &content,
                    const std::string &file_ext,
                    std::string &file_id);

        /**
         * @brief 上传（自动从文件名提取扩展）
         */
        bool upload(const std::string &content,
                    const std::string &filename,
                    std::string &file_id,
                    bool unused);

        /**
         * @brief 下载到内存缓冲区
         * @param file_id  FastDFS file_id
         * @param content  输出：文件内容
         * @return 是否成功
         */
        bool download(const std::string &file_id,
                      std::string &content);

        /**
         * @brief 删除文件
         * @param file_id FastDFS file_id
         * @return 是否成功
         */
        bool remove(const std::string &file_id);

        /**
         * @brief 获取错误消息
         */
        std::string lastError() const { return last_error_; }

    private:
        bool initialized_{false};
        std::string last_error_;

        std::string getFileExtension(const std::string &filename);
    };

} // namespace fileagent
