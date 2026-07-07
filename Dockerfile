# FileAgent Server — Docker 多阶段构建
#
# 构建阶段：安装所有编译依赖并编译
# 运行阶段：仅包含运行时依赖

# === 构建阶段 ===
FROM ubuntu:22.04 AS builder

LABEL stage=builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    libmysqlclient-dev libfcgi-dev nginx \
    libhiredis-dev libyaml-cpp-dev \
    poppler-utils \
    && rm -rf /var/lib/apt/lists/*

# 安装 vcpkg 及剩余依赖
RUN git clone https://github.com/Microsoft/vcpkg /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# 复制依赖清单
COPY vcpkg.json /opt/vcpkg/
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# 复制 agents-cpp 预编译库
COPY lib/agents-cpp /opt/agents-cpp

WORKDIR /build
COPY . .

# 构建
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DAGENTS_CPP_DIR=/opt/agents-cpp \
    && cmake --build build -j$(nproc)

# === 运行阶段 ===
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libmysqlclient21 libfcgi-bin \
    libhiredis1.1.0 \
    nginx poppler-utils ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 复制构建产物
COPY --from=builder /build/build/fileagent-server /usr/local/bin/
COPY --from=builder /build/nginx.conf /etc/nginx/sites-available/fileagent
COPY --from=builder /build/config.yaml /etc/fileagent/config.yaml

# 复制 agents-cpp 运行时库
COPY --from=builder /opt/agents-cpp/lib/linux/x64/libagents_cpp_shared_lib.so /usr/local/lib/
RUN ldconfig

# 配置 Nginx
RUN ln -sf /etc/nginx/sites-available/fileagent /etc/nginx/sites-enabled/ && \
    rm -f /etc/nginx/sites-enabled/default

# 创建数据目录
RUN mkdir -p /data/uploads /var/log/fileagent

WORKDIR /etc/fileagent

EXPOSE 80

# 启动脚本：先启动 Nginx，再启动 FastCGI 服务
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENTRYPOINT ["docker-entrypoint.sh"]
