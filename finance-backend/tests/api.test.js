'use strict';

/**
 * Integration tests for the Finance Backend API.
 * Uses Node.js built-in test runner (node:test).
 * Run with: npm test
 */

const { describe, it, before, after, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const http = require('node:http');

// Use an in-memory database for tests
process.env.DB_PATH = ':memory:';
process.env.JWT_SECRET = 'test-secret';

const app = require('../src/app');

let server;
let baseUrl;

// ── Helpers ──────────────────────────────────────────────────────────────────

function request(method, path, { body, token } = {}) {
  return new Promise((resolve, reject) => {
    const url = new URL(path, baseUrl);
    const data = body ? JSON.stringify(body) : undefined;
    const headers = { 'Content-Type': 'application/json' };
    if (token) headers['Authorization'] = `Bearer ${token}`;

    const req = http.request(
      { hostname: url.hostname, port: url.port, path: url.pathname + url.search, method, headers },
      (res) => {
        let raw = '';
        res.on('data', c => raw += c);
        res.on('end', () => {
          try {
            resolve({ status: res.statusCode, body: JSON.parse(raw) });
          } catch {
            resolve({ status: res.statusCode, body: raw });
          }
        });
      }
    );
    req.on('error', reject);
    if (data) req.write(data);
    req.end();
  });
}

before((ctx, done) => {
  server = http.createServer(app);
  server.listen(0, '127.0.0.1', () => {
    const { port } = server.address();
    baseUrl = `http://127.0.0.1:${port}`;
    done();
  });
});

after((ctx, done) => server.close(done));

// ── Auth tests ────────────────────────────────────────────────────────────────

describe('Auth', () => {
  it('registers a new user and returns a JWT', async () => {
    const res = await request('POST', '/api/auth/register', {
      body: { username: 'testadmin', email: 'testadmin@example.com', password: 'Admin@1234' },
    });
    assert.equal(res.status, 201);
    assert.ok(res.body.token);
    assert.equal(res.body.user.role, 'viewer');
  });

  it('rejects duplicate registration', async () => {
    await request('POST', '/api/auth/register', {
      body: { username: 'dupuser', email: 'dup@example.com', password: 'Admin@1234' },
    });
    const res = await request('POST', '/api/auth/register', {
      body: { username: 'dupuser', email: 'dup@example.com', password: 'Admin@1234' },
    });
    assert.equal(res.status, 409);
  });

  it('rejects weak passwords', async () => {
    const res = await request('POST', '/api/auth/register', {
      body: { username: 'weakpw', email: 'weak@example.com', password: 'short' },
    });
    assert.equal(res.status, 422);
  });

  it('logs in with correct credentials', async () => {
    await request('POST', '/api/auth/register', {
      body: { username: 'logintest', email: 'login@example.com', password: 'Login@1234' },
    });
    const res = await request('POST', '/api/auth/login', {
      body: { username: 'logintest', password: 'Login@1234' },
    });
    assert.equal(res.status, 200);
    assert.ok(res.body.token);
  });

  it('rejects wrong password', async () => {
    const res = await request('POST', '/api/auth/login', {
      body: { username: 'logintest', password: 'WrongPw@1' },
    });
    assert.equal(res.status, 401);
  });
});

// ── Records tests ────────────────────────────────────────────────────────────

describe('Records', () => {
  let adminToken;
  let viewerToken;

  before(async () => {
    // Create an admin user
    await request('POST', '/api/auth/register', {
      body: { username: 'recadmin', email: 'recadmin@example.com', password: 'Admin@1234' },
    });
    // We need the DB to promote them to admin. Do it via the users service directly.
    const { getDb } = require('../src/db/database');
    getDb().prepare("UPDATE users SET role = 'admin' WHERE username = 'recadmin'").run();

    const adminRes = await request('POST', '/api/auth/login', {
      body: { username: 'recadmin', password: 'Admin@1234' },
    });
    adminToken = adminRes.body.token;

    // Viewer user
    await request('POST', '/api/auth/register', {
      body: { username: 'recviewer', email: 'recviewer@example.com', password: 'Viewer@1234' },
    });
    const viewerRes = await request('POST', '/api/auth/login', {
      body: { username: 'recviewer', password: 'Viewer@1234' },
    });
    viewerToken = viewerRes.body.token;
  });

  it('creates a record (admin)', async () => {
    const res = await request('POST', '/api/records', {
      token: adminToken,
      body: { amount: 1000, type: 'income', category: 'Salary', date: '2024-01-15' },
    });
    assert.equal(res.status, 201);
    assert.equal(res.body.amount, 1000);
    assert.equal(res.body.type, 'income');
  });

  it('rejects record creation for viewer', async () => {
    const res = await request('POST', '/api/records', {
      token: viewerToken,
      body: { amount: 500, type: 'expense', category: 'Food', date: '2024-01-16' },
    });
    assert.equal(res.status, 403);
  });

  it('viewer can list records', async () => {
    const res = await request('GET', '/api/records', { token: viewerToken });
    assert.equal(res.status, 200);
    assert.ok(Array.isArray(res.body.records));
  });

  it('filters records by type', async () => {
    const res = await request('GET', '/api/records?type=income', { token: viewerToken });
    assert.equal(res.status, 200);
    assert.ok(res.body.records.every(r => r.type === 'income'));
  });

  it('rejects invalid record data', async () => {
    const res = await request('POST', '/api/records', {
      token: adminToken,
      body: { amount: -5, type: 'invalid', category: '', date: 'not-a-date' },
    });
    assert.equal(res.status, 422);
  });

  it('updates a record (admin)', async () => {
    const createRes = await request('POST', '/api/records', {
      token: adminToken,
      body: { amount: 200, type: 'expense', category: 'Food', date: '2024-02-01' },
    });
    const id = createRes.body.id;
    const updateRes = await request('PUT', `/api/records/${id}`, {
      token: adminToken,
      body: { amount: 250 },
    });
    assert.equal(updateRes.status, 200);
    assert.equal(updateRes.body.amount, 250);
  });

  it('soft-deletes a record (admin)', async () => {
    const createRes = await request('POST', '/api/records', {
      token: adminToken,
      body: { amount: 100, type: 'expense', category: 'Misc', date: '2024-03-01' },
    });
    const id = createRes.body.id;
    const deleteRes = await request('DELETE', `/api/records/${id}`, { token: adminToken });
    assert.equal(deleteRes.status, 204);
    const getRes = await request('GET', `/api/records/${id}`, { token: viewerToken });
    assert.equal(getRes.status, 404);
  });
});

// ── Dashboard tests ──────────────────────────────────────────────────────────

describe('Dashboard', () => {
  let analystToken;
  let viewerToken;

  before(async () => {
    await request('POST', '/api/auth/register', {
      body: { username: 'dashanalyst', email: 'dashanalyst@example.com', password: 'Analyst@1234' },
    });
    const { getDb } = require('../src/db/database');
    getDb().prepare("UPDATE users SET role = 'analyst' WHERE username = 'dashanalyst'").run();
    const r = await request('POST', '/api/auth/login', {
      body: { username: 'dashanalyst', password: 'Analyst@1234' },
    });
    analystToken = r.body.token;

    await request('POST', '/api/auth/register', {
      body: { username: 'dashviewer', email: 'dashviewer@example.com', password: 'Viewer@1234' },
    });
    const r2 = await request('POST', '/api/auth/login', {
      body: { username: 'dashviewer', password: 'Viewer@1234' },
    });
    viewerToken = r2.body.token;
  });

  it('analyst can access summary', async () => {
    const res = await request('GET', '/api/dashboard/summary', { token: analystToken });
    assert.equal(res.status, 200);
    assert.ok('total_income' in res.body);
    assert.ok('total_expenses' in res.body);
    assert.ok('net_balance' in res.body);
  });

  it('viewer is denied summary', async () => {
    const res = await request('GET', '/api/dashboard/summary', { token: viewerToken });
    assert.equal(res.status, 403);
  });

  it('analyst can access category breakdown', async () => {
    const res = await request('GET', '/api/dashboard/category-breakdown', { token: analystToken });
    assert.equal(res.status, 200);
    assert.ok(Array.isArray(res.body));
  });

  it('analyst can access monthly trends', async () => {
    const res = await request('GET', '/api/dashboard/trends', { token: analystToken });
    assert.equal(res.status, 200);
    assert.ok(Array.isArray(res.body));
  });

  it('viewer can access recent activity', async () => {
    const res = await request('GET', '/api/dashboard/recent', { token: viewerToken });
    assert.equal(res.status, 200);
    assert.ok(Array.isArray(res.body));
  });
});

// ── Users tests ──────────────────────────────────────────────────────────────

describe('Users', () => {
  let adminToken;
  let viewerToken;

  before(async () => {
    await request('POST', '/api/auth/register', {
      body: { username: 'useradmin', email: 'useradmin@example.com', password: 'Admin@1234' },
    });
    const { getDb } = require('../src/db/database');
    getDb().prepare("UPDATE users SET role = 'admin' WHERE username = 'useradmin'").run();
    const r = await request('POST', '/api/auth/login', {
      body: { username: 'useradmin', password: 'Admin@1234' },
    });
    adminToken = r.body.token;

    await request('POST', '/api/auth/register', {
      body: { username: 'managedviewer', email: 'mviewer@example.com', password: 'Viewer@1234' },
    });
    const r2 = await request('POST', '/api/auth/login', {
      body: { username: 'managedviewer', password: 'Viewer@1234' },
    });
    viewerToken = r2.body.token;
  });

  it('admin can list users', async () => {
    const res = await request('GET', '/api/users', { token: adminToken });
    assert.equal(res.status, 200);
    assert.ok(Array.isArray(res.body.users));
  });

  it('viewer cannot list users', async () => {
    const res = await request('GET', '/api/users', { token: viewerToken });
    assert.equal(res.status, 403);
  });

  it('admin can update user role', async () => {
    const listRes = await request('GET', '/api/users', { token: adminToken });
    const target = listRes.body.users.find(u => u.username === 'managedviewer');
    const res = await request('PUT', `/api/users/${target.id}`, {
      token: adminToken,
      body: { role: 'analyst' },
    });
    assert.equal(res.status, 200);
    assert.equal(res.body.role, 'analyst');
  });

  it('returns 404 for unknown user', async () => {
    const res = await request('GET', '/api/users/99999', { token: adminToken });
    assert.equal(res.status, 404);
  });
});
