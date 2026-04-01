'use strict';

const bcrypt = require('bcryptjs');
const { getDb } = require('../db/database');
const { signToken } = require('../middleware/auth');

const SALT_ROUNDS = 10;

function register({ username, email, password }) {
  const db = getDb();

  const existing = db
    .prepare('SELECT id FROM users WHERE username = ? OR email = ?')
    .get(username, email);
  if (existing) {
    const err = new Error('Username or email already taken.');
    err.status = 409;
    throw err;
  }

  const passwordHash = bcrypt.hashSync(password, SALT_ROUNDS);
  const result = db
    .prepare(`
      INSERT INTO users (username, email, password_hash, role)
      VALUES (@username, @email, @passwordHash, 'viewer')
    `)
    .run({ username, email, passwordHash });

  const user = db.prepare('SELECT id, username, email, role, status, created_at FROM users WHERE id = ?').get(result.lastInsertRowid);
  return { token: signToken(user), user };
}

function login({ username, password }) {
  const db = getDb();

  const user = db
    .prepare('SELECT * FROM users WHERE username = ?')
    .get(username);

  if (!user) {
    const err = new Error('Invalid credentials.');
    err.status = 401;
    throw err;
  }

  if (user.status === 'inactive') {
    const err = new Error('Account is deactivated. Contact an administrator.');
    err.status = 403;
    throw err;
  }

  const match = bcrypt.compareSync(password, user.password_hash);
  if (!match) {
    const err = new Error('Invalid credentials.');
    err.status = 401;
    throw err;
  }

  const { password_hash, ...safeUser } = user; // eslint-disable-line no-unused-vars
  return { token: signToken(safeUser), user: safeUser };
}

module.exports = { register, login };
