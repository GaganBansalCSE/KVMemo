'use strict';

/**
 * Seed script — creates demo users and sample financial records.
 * Run with: npm run seed
 */

const bcrypt = require('bcryptjs');
const { getDb } = require('./database');

const SALT_ROUNDS = 10;

async function seed() {
  const db = getDb();

  // ── Users ──────────────────────────────────────────────────────────────────
  const users = [
    { username: 'admin',   email: 'admin@example.com',   role: 'admin',   password: 'Admin@123'   },
    { username: 'analyst', email: 'analyst@example.com', role: 'analyst', password: 'Analyst@123' },
    { username: 'viewer',  email: 'viewer@example.com',  role: 'viewer',  password: 'Viewer@123'  },
  ];

  const insertUser = db.prepare(`
    INSERT OR IGNORE INTO users (username, email, password_hash, role)
    VALUES (@username, @email, @password_hash, @role)
  `);

  for (const u of users) {
    const hash = await bcrypt.hash(u.password, SALT_ROUNDS);
    insertUser.run({ username: u.username, email: u.email, password_hash: hash, role: u.role });
  }

  const adminUser = db.prepare('SELECT id FROM users WHERE username = ?').get('admin');
  if (!adminUser) {
    console.error('Admin user not found after seed — aborting record seed.');
    process.exit(1);
  }

  // ── Financial records ──────────────────────────────────────────────────────
  const insertRecord = db.prepare(`
    INSERT INTO financial_records (amount, type, category, date, notes, created_by)
    VALUES (@amount, @type, @category, @date, @notes, @created_by)
  `);

  const records = [
    { amount: 5000.00, type: 'income',  category: 'Salary',       date: '2024-01-15', notes: 'January salary' },
    { amount: 1200.50, type: 'expense', category: 'Rent',         date: '2024-01-01', notes: 'Monthly rent' },
    { amount:  350.00, type: 'expense', category: 'Utilities',    date: '2024-01-10', notes: 'Electric and water' },
    { amount: 5000.00, type: 'income',  category: 'Salary',       date: '2024-02-15', notes: 'February salary' },
    { amount: 1200.50, type: 'expense', category: 'Rent',         date: '2024-02-01', notes: 'Monthly rent' },
    { amount:  200.00, type: 'expense', category: 'Groceries',    date: '2024-02-20', notes: null },
    { amount: 5500.00, type: 'income',  category: 'Salary',       date: '2024-03-15', notes: 'March salary + bonus' },
    { amount: 1200.50, type: 'expense', category: 'Rent',         date: '2024-03-01', notes: 'Monthly rent' },
    { amount:  800.00, type: 'expense', category: 'Electronics',  date: '2024-03-25', notes: 'New keyboard' },
    { amount:  750.00, type: 'income',  category: 'Freelance',    date: '2024-03-28', notes: 'Web design project' },
    { amount:  120.00, type: 'expense', category: 'Subscriptions',date: '2024-03-05', notes: 'Cloud services' },
    { amount: 5000.00, type: 'income',  category: 'Salary',       date: '2024-04-15', notes: 'April salary' },
    { amount: 1200.50, type: 'expense', category: 'Rent',         date: '2024-04-01', notes: 'Monthly rent' },
    { amount:  300.00, type: 'expense', category: 'Travel',       date: '2024-04-10', notes: 'Weekend trip' },
    { amount: 1000.00, type: 'income',  category: 'Freelance',    date: '2024-04-22', notes: 'Logo design' },
  ];

  const seedRecords = db.transaction(() => {
    for (const r of records) {
      insertRecord.run({ ...r, created_by: adminUser.id });
    }
  });
  seedRecords();

  console.log('✅  Seed complete.');
  console.log('\nDemo credentials:');
  users.forEach(u => console.log(`  ${u.role.padEnd(8)} → username: ${u.username}, password: (set during seed)`));
}

seed().catch(err => { console.error(err); process.exit(1); });
