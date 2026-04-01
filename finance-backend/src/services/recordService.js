'use strict';

const { getDb } = require('../db/database');

/**
 * Build a WHERE clause fragment and params array for common record filters.
 */
function buildFilters({ type, category, from_date, to_date }) {
  const conditions = ['deleted_at IS NULL'];
  const params = [];

  if (type) {
    conditions.push('type = ?');
    params.push(type);
  }
  if (category) {
    conditions.push('LOWER(category) = LOWER(?)');
    params.push(category);
  }
  if (from_date) {
    conditions.push('date >= ?');
    params.push(from_date);
  }
  if (to_date) {
    conditions.push('date <= ?');
    params.push(to_date);
  }

  return { where: conditions.join(' AND '), params };
}

function list({ type, category, from_date, to_date, page = 1, limit = 20 } = {}) {
  const db = getDb();
  const { where, params } = buildFilters({ type, category, from_date, to_date });

  const total = db
    .prepare(`SELECT COUNT(*) AS count FROM financial_records WHERE ${where}`)
    .get(...params).count;

  const records = db
    .prepare(`
      SELECT fr.*, u.username AS created_by_username
      FROM financial_records fr
      JOIN users u ON u.id = fr.created_by
      WHERE ${where}
      ORDER BY fr.date DESC, fr.created_at DESC
      LIMIT ? OFFSET ?
    `)
    .all(...params, limit, (page - 1) * limit);

  return { total, page, limit, records };
}

function getById(id) {
  const db = getDb();
  const record = db
    .prepare(`
      SELECT fr.*, u.username AS created_by_username
      FROM financial_records fr
      JOIN users u ON u.id = fr.created_by
      WHERE fr.id = ? AND fr.deleted_at IS NULL
    `)
    .get(id);

  if (!record) {
    const err = new Error('Record not found.');
    err.status = 404;
    throw err;
  }
  return record;
}

function create({ amount, type, category, date, notes }, createdBy) {
  const db = getDb();
  const dateStr = date instanceof Date ? date.toISOString().slice(0, 10) : date;

  const result = db
    .prepare(`
      INSERT INTO financial_records (amount, type, category, date, notes, created_by)
      VALUES (@amount, @type, @category, @date, @notes, @created_by)
    `)
    .run({ amount, type, category, date: dateStr, notes: notes || null, created_by: createdBy });

  return getById(result.lastInsertRowid);
}

function update(id, { amount, type, category, date, notes }) {
  const db = getDb();

  const existing = db
    .prepare('SELECT id FROM financial_records WHERE id = ? AND deleted_at IS NULL')
    .get(id);
  if (!existing) {
    const err = new Error('Record not found.');
    err.status = 404;
    throw err;
  }

  const fields = [];
  const params = {};

  if (amount !== undefined)   { fields.push('amount = @amount');       params.amount   = amount; }
  if (type !== undefined)     { fields.push('type = @type');           params.type     = type;   }
  if (category !== undefined) { fields.push('category = @category');   params.category = category; }
  if (date !== undefined) {
    fields.push('date = @date');
    params.date = date instanceof Date ? date.toISOString().slice(0, 10) : date;
  }
  if (notes !== undefined)    { fields.push('notes = @notes');         params.notes    = notes ?? null; }

  if (fields.length === 0) {
    const err = new Error('No valid fields provided for update.');
    err.status = 400;
    throw err;
  }

  fields.push("updated_at = datetime('now')");
  params.id = id;

  db.prepare(`UPDATE financial_records SET ${fields.join(', ')} WHERE id = @id`).run(params);

  return getById(id);
}

function softDelete(id) {
  const db = getDb();
  const result = db
    .prepare(`UPDATE financial_records SET deleted_at = datetime('now') WHERE id = ? AND deleted_at IS NULL`)
    .run(id);

  if (result.changes === 0) {
    const err = new Error('Record not found.');
    err.status = 404;
    throw err;
  }
}

module.exports = { list, getById, create, update, softDelete };
