'use strict';

const express = require('express');

const authRoutes      = require('./routes/auth');
const userRoutes      = require('./routes/users');
const recordRoutes    = require('./routes/records');
const dashboardRoutes = require('./routes/dashboard');
const { errorHandler } = require('./middleware/errorHandler');

const app = express();

// ── Body parsing ────────────────────────────────────────────────────────────
app.use(express.json());

// ── Health check ─────────────────────────────────────────────────────────────
app.get('/health', (_req, res) => res.json({ status: 'ok' }));

// ── API Routes ────────────────────────────────────────────────────────────────
app.use('/api/auth',      authRoutes);
app.use('/api/users',     userRoutes);
app.use('/api/records',   recordRoutes);
app.use('/api/dashboard', dashboardRoutes);

// ── 404 handler ───────────────────────────────────────────────────────────────
app.use((_req, res) => res.status(404).json({ error: 'Route not found.' }));

// ── Global error handler (must be last) ───────────────────────────────────────
app.use(errorHandler);

// ── Start server unless imported as a module (e.g., during tests) ─────────────
if (require.main === module) {
  const PORT = process.env.PORT || 3000;
  app.listen(PORT, () => {
    console.log(`Finance backend listening on http://localhost:${PORT}`);
    console.log(`  POST /api/auth/register`);
    console.log(`  POST /api/auth/login`);
    console.log(`  GET  /api/records`);
    console.log(`  GET  /api/dashboard/summary`);
  });
}

module.exports = app;
