#!/bin/bash
set -e

# FileAgent Server — Docker 入口脚本
# 启动 Nginx + FastCGI 服务

# 替换配置文件中的数据库连接信息（支持 Docker 环境变量）
if [ -n "$DB_HOST" ]; then
    sed -i "s/host: \"127.0.0.1\"/host: \"$DB_HOST\"/" /etc/fileagent/config.yaml
fi
if [ -n "$DB_PORT" ]; then
    sed -i "s/port: 3306/port: $DB_PORT/" /etc/fileagent/config.yaml
fi
if [ -n "$DB_USER" ]; then
    sed -i "s/user: \"fileagent\"/user: \"$DB_USER\"/" /etc/fileagent/config.yaml
fi
if [ -n "$DB_PASSWORD" ]; then
    sed -i "s/password: \"fileagent123\"/password: \"$DB_PASSWORD\"/" /etc/fileagent/config.yaml
fi
if [ -n "$DB_NAME" ]; then
    sed -i "s/database: \"fileagent\"/database: \"$DB_NAME\"/" /etc/fileagent/config.yaml
fi

# Redis 配置
if [ -n "$REDIS_HOST" ]; then
    sed -i "s/host: \"127.0.0.1\"/host: \"$REDIS_HOST\"/" /etc/fileagent/config.yaml
fi

# 等待数据库就绪
if [ -n "$DB_HOST" ]; then
    echo "Waiting for database $DB_HOST:$DB_PORT..."
    for i in $(seq 1 30); do
        if mysqladmin ping -h"$DB_HOST" -P"${DB_PORT:-3306}" -u"${DB_USER:-fileagent}" -p"${DB_PASSWORD:-fileagent123}" --silent 2>/dev/null; then
            echo "Database ready"
            break
        fi
        echo "Waiting... ($i/30)"
        sleep 2
    done
fi

# 初始化数据库（首次运行）
if [ -n "$DB_HOST" ] && [ -f /sql/init.sql ]; then
    echo "Initializing database..."
    mysql -h"$DB_HOST" -P"${DB_PORT:-3306}" -u"${DB_USER:-fileagent}" -p"${DB_PASSWORD:-fileagent123}" < /sql/init.sql 2>/dev/null || true
fi

# 确保上传目录存在
mkdir -p /data/uploads
mkdir -p /var/log/fileagent

# 启动 Nginx
echo "Starting Nginx..."
nginx -g "daemon off;" &
NGINX_PID=$!

# 启动 FastCGI 服务
echo "Starting FileAgent Server..."
exec /usr/local/bin/fileagent-server /etc/fileagent/config.yaml
