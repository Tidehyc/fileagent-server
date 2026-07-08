/* FileAgent Server — API 调用封装 */

const API = {
  // 从 localStorage 获取 token
  getToken() {
    return localStorage.getItem('token');
  },

  setToken(token) {
    localStorage.setItem('token', token);
  },

  clearToken() {
    localStorage.removeItem('token');
  },

  isLoggedIn() {
    return !!this.getToken();
  },

  // 通用请求
  async request(method, path, body = null) {
    const headers = { 'Content-Type': 'application/json' };
    const token = this.getToken();
    if (token) {
      headers['Authorization'] = 'Bearer ' + token;
    }

    const opts = { method, headers };
    if (body) {
      opts.body = JSON.stringify(body);
    }

    const res = await fetch(path, opts);
    const data = await res.json();
    if (data.code !== 0 && data.code !== 200) {
      throw new Error(data.message || '请求失败');
    }
    return data;
  },

  // 上传文件（multipart）
  async uploadFile(file) {
    const token = this.getToken();
    const formData = new FormData();
    formData.append('file', file);

    const res = await fetch('/api/files/upload', {
      method: 'POST',
      headers: token ? { 'Authorization': 'Bearer ' + token } : {},
      body: formData
    });
    return await res.json();
  },

  // ─── 用户 ──────────────────────────────────────

  register(username, password, email) {
    return this.request('POST', '/api/user/register', { username, password, email });
  },

  async login(username, password) {
    const data = await this.request('POST', '/api/user/login', { username, password });
    if (data.token) this.setToken(data.token);
    return data;
  },

  logout() {
    const result = this.request('POST', '/api/user/logout');
    this.clearToken();
    return result;
  },

  // ─── 文件 ──────────────────────────────────────

  listFiles(limit = 20, offset = 0) {
    return this.request('GET', `/api/files?limit=${limit}&offset=${offset}`);
  },

  getFile(fileId) {
    return this.request('GET', `/api/files/${fileId}`);
  },

  deleteFile(fileId) {
    return this.request('DELETE', `/api/files/${fileId}`);
  },

  checkFile(hash) {
    return this.request('POST', '/api/files/check', { file_hash: hash });
  },

  // ─── 分享 ──────────────────────────────────────

  createShare(fileId, expireHours = 0) {
    return this.request('POST', '/api/shares', { file_id: fileId, expire_hours: expireHours });
  },

  listShares() {
    return this.request('GET', '/api/shares');
  },

  deleteShare(token) {
    return this.request('DELETE', `/api/shares/${token}`);
  },

  saveShare(token) {
    return this.request('POST', `/api/shares/${token}/save`);
  },

  getRanking(limit = 10) {
    return this.request('GET', `/api/shares/ranking?limit=${limit}`);
  },

  // ─── 搜索 ──────────────────────────────────────

  search(query, limit = 10) {
    return this.request('POST', '/api/search', { query, limit });
  },

  // ─── Agent ─────────────────────────────────────

  agentChat(message) {
    return this.request('POST', '/api/agent/chat', { message });
  },

  // ─── 健康检查 ──────────────────────────────────

  health() {
    return this.request('GET', '/health');
  },

  // 工具函数
  formatSize(bytes) {
    if (!bytes) return '0 B';
    const units = ['B', 'KB', 'MB', 'GB'];
    let i = 0;
    let size = bytes;
    while (size >= 1024 && i < units.length - 1) {
      size /= 1024;
      i++;
    }
    return size.toFixed(i > 0 ? 1 : 0) + ' ' + units[i];
  },

  formatDate(str) {
    if (!str) return '-';
    return str.replace('T', ' ').substring(0, 19);
  },

  copyToClipboard(text) {
    navigator.clipboard.writeText(text).catch(() => {});
  }
};
