'use strict';

const { Router } = require('express');
const { authenticate, authorize } = require('../middleware/auth');
const dashboardService = require('../services/dashboardService');

const router = Router();

// All dashboard routes require authentication.
router.use(authenticate);

/**
 * GET /api/dashboard/summary?from_date=YYYY-MM-DD&to_date=YYYY-MM-DD
 * Returns total income, total expenses, net balance, record count.
 * Roles: analyst, admin
 */
router.get('/summary', authorize('analyst', 'admin'), (req, res, next) => {
  try {
    const { from_date, to_date } = req.query;
    res.json(dashboardService.summary({ from_date, to_date }));
  } catch (err) {
    next(err);
  }
});

/**
 * GET /api/dashboard/category-breakdown?type=income&from_date=...&to_date=...
 * Returns category-wise totals, averages, min/max.
 * Roles: analyst, admin
 */
router.get('/category-breakdown', authorize('analyst', 'admin'), (req, res, next) => {
  try {
    const { type, from_date, to_date } = req.query;
    res.json(dashboardService.categoryBreakdown({ type, from_date, to_date }));
  } catch (err) {
    next(err);
  }
});

/**
 * GET /api/dashboard/trends?from_date=...&to_date=...
 * Returns monthly income and expense totals.
 * Roles: analyst, admin
 */
router.get('/trends', authorize('analyst', 'admin'), (req, res, next) => {
  try {
    const { from_date, to_date } = req.query;
    res.json(dashboardService.monthlyTrends({ from_date, to_date }));
  } catch (err) {
    next(err);
  }
});

/**
 * GET /api/dashboard/recent?limit=10
 * Returns the N most recent records.
 * Roles: viewer, analyst, admin
 */
router.get('/recent', (req, res, next) => {
  try {
    const limit = Math.min(50, Math.max(1, parseInt(req.query.limit, 10) || 10));
    res.json(dashboardService.recentActivity({ limit }));
  } catch (err) {
    next(err);
  }
});

module.exports = router;
