'use strict';

const { getDb } = require('../db/database');

const SAFE_COLUMNS = 'id, username, email, role, status, created_at, updated_at';

function list({ page = 1, limit = 20 } = {}) {
  const db = getDb();
  const offset = (page - 1) * limit;
  const total = db.prepare('SELECT COUNT(*) AS count FROM users').get().count;
  const users = db
    .prepare(`SELECT ${SAFE_COLUMNS} FROM users ORDER BY created_at DESC LIMIT ? OFFSET ?`)
    .all(limit, offset);
  return { total, page, limit, users };
}

function getById(id) {
  const db = getDb();
  const user = db
    .prepare(`SELECT ${SAFE_COLUMNS} FROM users WHERE id = ?`)
    .get(id);
  if (!user) {
    const err = new Error('User not found.');
    err.status = 404;
    throw err;
  }
  return user;
}

function update(id, { role, status, email }) {
  const db = getDb();

  const user = db.prepare('SELECT id FROM users WHERE id = ?').get(id);
  if (!user) {
    const err = new Error('User not found.');
    err.status = 404;
    throw err;
  }

  const fields = [];
  const params = {};

  if (role !== undefined)   { fields.push('role = @role');     params.role   = role;   }
  if (status !== undefined) { fields.push('status = @status'); params.status = status; }
  if (email !== undefined)  { fields.push('email = @email');   params.email  = email;  }

  if (fields.length === 0) {
    const err = new Error('No valid fields provided for update.');
    err.status = 400;
    throw err;
  }

  fields.push("updated_at = datetime('now')");
  params.id = id;

  db.prepare(`UPDATE users SET ${fields.join(', ')} WHERE id = @id`).run(params);

  return getById(id);
}

function remove(id) {
  const db = getDb();
  const result = db.prepare('DELETE FROM users WHERE id = ?').run(id);
  if (result.changes === 0) {
    const err = new Error('User not found.');
    err.status = 404;
    throw err;
  }
}

module.exports = { list, getById, update, remove };
