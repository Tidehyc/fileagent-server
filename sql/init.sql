-- FileAgent Server 数据库初始化
-- 使用方法: mysql -u root -p < sql/init.sql

CREATE DATABASE IF NOT EXISTS fileagent
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE fileagent;

-- ──────────────────────────────────────────────
-- 用户表
-- ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS users (
    id          BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '用户ID',
    username    VARCHAR(64)     NOT NULL                 COMMENT '用户名',
    password    VARCHAR(256)    NOT NULL                 COMMENT '密码（bcrypt 哈希）',
    email       VARCHAR(128)    NOT NULL                 COMMENT '邮箱',
    status      TINYINT         NOT NULL DEFAULT 1       COMMENT '状态: 1=正常, 0=禁用',
    is_admin    TINYINT         NOT NULL DEFAULT 0       COMMENT '管理员: 1=是, 0=否',
    created_at  DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at  DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',

    PRIMARY KEY (id),
    UNIQUE KEY uk_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户表';

-- ──────────────────────────────────────────────
-- 文件表
-- ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS files (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '文件ID',
    user_id         BIGINT          NOT NULL                 COMMENT '所属用户ID',
    filename        VARCHAR(256)    NOT NULL                 COMMENT '原始文件名',
    file_hash       VARCHAR(128)    NOT NULL                 COMMENT '文件 SHA-256 哈希',
    file_size       BIGINT          NOT NULL DEFAULT 0       COMMENT '文件大小（字节）',
    storage_path    VARCHAR(512)    NOT NULL                 COMMENT '存储路径',
    status          TINYINT         NOT NULL DEFAULT 1       COMMENT '状态: 1=正常, 0=删除',
    summary         TEXT                                     COMMENT 'AI 生成的文件摘要',
    tags            VARCHAR(512)                             COMMENT 'AI 生成的标签（逗号分隔）',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (id),
    KEY idx_user_id (user_id),
    KEY idx_file_hash (file_hash),
    FULLTEXT idx_ft_summary (summary),
    FULLTEXT idx_ft_tags (tags),
    CONSTRAINT fk_files_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='文件表';

-- ──────────────────────────────────────────────
-- 文件分片表（大文件分片上传）
-- ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS file_chunks (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '分片ID',
    file_id         BIGINT          NOT NULL                 COMMENT '所属文件ID',
    chunk_index     INT             NOT NULL                 COMMENT '分片序号（从0开始）',
    chunk_hash      VARCHAR(128)    NOT NULL                 COMMENT '分片 SHA-256 哈希',
    chunk_size      BIGINT          NOT NULL                 COMMENT '分片大小（字节）',
    storage_path    VARCHAR(512)    NOT NULL                 COMMENT '分片存储路径',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (id),
    UNIQUE KEY uk_file_chunk (file_id, chunk_index),
    KEY idx_chunk_hash (chunk_hash),
    CONSTRAINT fk_chunks_file FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='文件分片表';

-- ──────────────────────────────────────────────
-- 分享链接表
-- ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS shares (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '分享ID',
    file_id         BIGINT          NOT NULL                 COMMENT '被分享的文件ID',
    user_id         BIGINT          NOT NULL                 COMMENT '分享者用户ID',
    share_token     VARCHAR(64)     NOT NULL                 COMMENT '分享令牌（唯一）',
    expires_at      DATETIME        DEFAULT NULL             COMMENT '过期时间（NULL=永不过期）',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (id),
    UNIQUE KEY uk_share_token (share_token),
    KEY idx_expires_at (expires_at),
    CONSTRAINT fk_shares_file FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    CONSTRAINT fk_shares_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='分享链接表';

-- ──────────────────────────────────────────────
-- 会话表（Session 认证）
-- ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS sessions (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '会话ID',
    user_id         BIGINT          NOT NULL                 COMMENT '用户ID',
    session_token   VARCHAR(128)    NOT NULL                 COMMENT '会话令牌',
    expires_at      DATETIME        NOT NULL                 COMMENT '过期时间',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (id),
    UNIQUE KEY uk_session_token (session_token),
    KEY idx_user_id (user_id),
    KEY idx_expires_at (expires_at),
    CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='会话（Session）表';

-- ──────────────────────────────────────────────
-- 搜索日志表
-- ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS search_logs (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '日志ID',
    user_id         BIGINT          NOT NULL                 COMMENT '用户ID',
    query           TEXT            NOT NULL                 COMMENT '搜索关键词',
    result_count    INT             NOT NULL DEFAULT 0       COMMENT '结果数',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '搜索时间',

    PRIMARY KEY (id),
    KEY idx_user_id (user_id),
    KEY idx_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='搜索日志表';

-- ──────────────────────────────────────────────
-- 数据库迁移（已有数据库升级用）
-- ──────────────────────────────────────────────
-- 如果是从旧版升级，取消注释以下语句：
-- ALTER TABLE files ADD COLUMN summary TEXT COMMENT 'AI 生成的文件摘要' AFTER status;
-- ALTER TABLE files ADD COLUMN tags VARCHAR(512) COMMENT 'AI 生成的标签' AFTER summary;
