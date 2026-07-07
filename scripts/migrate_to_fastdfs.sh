#!/bin/bash
#
# 本地文件 → FastDFS 迁移脚本
# 将现有 uploads/ 目录中的文件导入 FastDFS，并更新数据库记录
#
# 使用方法:
#   cd /home/tide/workplace/FileAgent_Server
#   bash scripts/migrate_to_fastdfs.sh
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
UPLOADS_DIR="$PROJECT_DIR/uploads"
FDFS_CONF="/etc/fdfs/client.conf"
MYSQL_CREDS="-u fileagent -pfileagent123"

# 数据库连接检查
echo "🔍 检查 MySQL 连接..."
mysql $MYSQL_CREDS -e "SELECT 1" fileagent > /dev/null 2>&1 || {
    echo "❌ MySQL 连接失败，请检查配置"
    exit 1
}

# FastDFS 连接检查
echo "🔍 检查 FastDFS 连接..."
fdfs_monitor $FDFS_CONF > /dev/null 2>&1 || {
    echo "❌ FastDFS 连接失败，请检查 tracker 是否运行"
    exit 1
}

echo ""
echo "============================================"
echo "  本地文件 → FastDFS 迁移"
echo "============================================"
echo ""

# 统计未迁移文件
COUNT=$(mysql $MYSQL_CREDS -N fileagent -e \
  "SELECT COUNT(*) FROM files WHERE storage_path LIKE 'uploads/%'")
echo "📊 待迁移文件数: $COUNT"
echo ""

if [ "$COUNT" -eq 0 ]; then
    echo "✅ 没有需要迁移的文件"
    exit 0
fi

# 逐文件迁移
SUCCESS=0
FAILED=0

mysql $MYSQL_CREDS -N fileagent -e \
  "SELECT id, filename, storage_path FROM files WHERE storage_path LIKE 'uploads/%' ORDER BY id" | \
while read -r ID FILENAME STORAGE_PATH; do
    LOCAL_PATH="$PROJECT_DIR/$STORAGE_PATH"

    echo -n "[$ID] $FILENAME → "

    if [ ! -f "$LOCAL_PATH" ]; then
        echo "⚠️  本地文件不存在，跳过 (id=$ID)"
        continue
    fi

    # 上传到 FastDFS
    FDFS_ID=$(fdfs_upload_file $FDFS_CONF "$LOCAL_PATH" 2>/dev/null)
    if [ -z "$FDFS_ID" ]; then
        echo "❌ FastDFS 上传失败 (id=$ID)"
        continue
    fi

    # 更新数据库
    mysql $MYSQL_CREDS fileagent -e \
      "UPDATE files SET storage_path='$FDFS_ID' WHERE id=$ID AND storage_path LIKE 'uploads/%'"

    echo "✅ $FDFS_ID"
done

echo ""
echo "============================================"
echo "  迁移完成"
echo "============================================"
