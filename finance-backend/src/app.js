'use strict';

const express = require('express');
const rateLimit = require('express-rate-limit');

const authRoutes      = require('./routes/auth');
const userRoutes      = require('./routes/users');
const recordRoutes    = require('./routes/records');
const dashboardRoutes = require('./routes/dashboard');
const { errorHandler } = require('./middleware/errorHandler');

const app = express();

// ── Body parsing ────────────────────────────────────────────────────────────
app.use(express.json());

// ── Rate limiting ─────────────────────────────────────────────────────────────
const authLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,  // 15 minutes
  max: 20,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: 'Too many requests, please try again later.' },
  skip: () => process.env.NODE_ENV === 'test',
});

const apiLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 200,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: 'Too many requests, please try again later.' },
  skip: () => process.env.NODE_ENV === 'test',
});

// ── Health check ─────────────────────────────────────────────────────────────
app.get('/health', (_req, res) => res.json({ status: 'ok' }));

// ── API Routes ────────────────────────────────────────────────────────────────
app.use('/api/auth',      authLimiter, authRoutes);
app.use('/api/users',     apiLimiter,  userRoutes);
app.use('/api/records',   apiLimiter,  recordRoutes);
app.use('/api/dashboard', apiLimiter,  dashboardRoutes);

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
