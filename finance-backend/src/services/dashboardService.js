'use strict';

const { getDb } = require('../db/database');

/**
 * Overall totals: total income, total expenses, net balance.
 */
function summary({ from_date, to_date } = {}) {
  const db = getDb();
  const conditions = ['deleted_at IS NULL'];
  const params = [];

  if (from_date) { conditions.push('date >= ?'); params.push(from_date); }
  if (to_date)   { conditions.push('date <= ?'); params.push(to_date); }

  const where = conditions.join(' AND ');

  const row = db
    .prepare(`
      SELECT
        COALESCE(SUM(CASE WHEN type = 'income'  THEN amount ELSE 0 END), 0) AS total_income,
        COALESCE(SUM(CASE WHEN type = 'expense' THEN amount ELSE 0 END), 0) AS total_expenses,
        COALESCE(SUM(CASE WHEN type = 'income'  THEN amount ELSE -amount END), 0) AS net_balance,
        COUNT(*) AS record_count
      FROM financial_records
      WHERE ${where}
    `)
    .get(...params);

  return row;
}

/**
 * Income and expense totals grouped by category.
 */
function categoryBreakdown({ type, from_date, to_date } = {}) {
  const db = getDb();
  const conditions = ['deleted_at IS NULL'];
  const params = [];

  if (type)      { conditions.push('type = ?');   params.push(type); }
  if (from_date) { conditions.push('date >= ?');  params.push(from_date); }
  if (to_date)   { conditions.push('date <= ?');  params.push(to_date); }

  const where = conditions.join(' AND ');

  return db
    .prepare(`
      SELECT
        category,
        type,
        COUNT(*)        AS record_count,
        SUM(amount)     AS total,
        AVG(amount)     AS average,
        MIN(amount)     AS min,
        MAX(amount)     AS max
      FROM financial_records
      WHERE ${where}
      GROUP BY category, type
      ORDER BY total DESC
    `)
    .all(...params);
}

/**
 * Monthly aggregates (year + month bucket).
 */
function monthlyTrends({ from_date, to_date } = {}) {
  const db = getDb();
  const conditions = ['deleted_at IS NULL'];
  const params = [];

  if (from_date) { conditions.push('date >= ?'); params.push(from_date); }
  if (to_date)   { conditions.push('date <= ?'); params.push(to_date); }

  const where = conditions.join(' AND ');

  return db
    .prepare(`
      SELECT
        strftime('%Y-%m', date)  AS month,
        type,
        COUNT(*)                 AS record_count,
        SUM(amount)              AS total
      FROM financial_records
      WHERE ${where}
      GROUP BY month, type
      ORDER BY month ASC
    `)
    .all(...params);
}

/**
 * N most recent (non-deleted) records.
 */
function recentActivity({ limit = 10 } = {}) {
  const db = getDb();
  return db
    .prepare(`
      SELECT fr.id, fr.amount, fr.type, fr.category, fr.date, fr.notes, fr.created_at,
             u.username AS created_by_username
      FROM financial_records fr
      JOIN users u ON u.id = fr.created_by
      WHERE fr.deleted_at IS NULL
      ORDER BY fr.date DESC, fr.created_at DESC
      LIMIT ?
    `)
    .all(limit);
}

module.exports = { summary, categoryBreakdown, monthlyTrends, recentActivity };
