'use strict';

const Database = require('better-sqlite3');
const path = require('path');
const fs = require('fs');

const DB_DIR = path.join(__dirname, '../../data');
const DB_PATH = process.env.DB_PATH || path.join(DB_DIR, 'finance.db');

let db;

function getDb() {
  if (!db) {
    if (!fs.existsSync(DB_DIR)) {
      fs.mkdirSync(DB_DIR, { recursive: true });
    }
    db = new Database(DB_PATH);
    db.pragma('journal_mode = WAL');
    db.pragma('foreign_keys = ON');
    initSchema(db);
  }
  return db;
}

function initSchema(database) {
  database.exec(`
    CREATE TABLE IF NOT EXISTS users (
      id            INTEGER PRIMARY KEY AUTOINCREMENT,
      username      TEXT    NOT NULL UNIQUE,
      email         TEXT    NOT NULL UNIQUE,
      password_hash TEXT    NOT NULL,
      role          TEXT    NOT NULL DEFAULT 'viewer'
                    CHECK(role IN ('viewer', 'analyst', 'admin')),
      status        TEXT    NOT NULL DEFAULT 'active'
                    CHECK(status IN ('active', 'inactive')),
      created_at    TEXT    NOT NULL DEFAULT (datetime('now')),
      updated_at    TEXT    NOT NULL DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS financial_records (
      id          INTEGER PRIMARY KEY AUTOINCREMENT,
      amount      REAL    NOT NULL CHECK(amount > 0),
      type        TEXT    NOT NULL CHECK(type IN ('income', 'expense')),
      category    TEXT    NOT NULL,
      date        TEXT    NOT NULL,
      notes       TEXT,
      created_by  INTEGER NOT NULL REFERENCES users(id),
      created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
      updated_at  TEXT    NOT NULL DEFAULT (datetime('now')),
      deleted_at  TEXT
    );

    CREATE INDEX IF NOT EXISTS idx_records_type     ON financial_records(type);
    CREATE INDEX IF NOT EXISTS idx_records_category ON financial_records(category);
    CREATE INDEX IF NOT EXISTS idx_records_date     ON financial_records(date);
    CREATE INDEX IF NOT EXISTS idx_records_deleted  ON financial_records(deleted_at);
  `);
}

module.exports = { getDb };
