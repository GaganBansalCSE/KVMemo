'use strict';

const { Router } = require('express');
const { authenticate, authorize } = require('../middleware/auth');
const { validate } = require('../middleware/validate');
const { create: createRules, update: updateRules } = require('../validators/records');
const recordService = require('../services/recordService');

const router = Router();

// All record routes require authentication.
router.use(authenticate);

/**
 * GET /api/records
 * Query params: type, category, from_date, to_date, page, limit
 * Roles: viewer, analyst, admin
 */
router.get('/', (req, res, next) => {
  try {
    const { type, category, from_date, to_date } = req.query;
    const page  = Math.max(1, parseInt(req.query.page,  10) || 1);
    const limit = Math.min(100, Math.max(1, parseInt(req.query.limit, 10) || 20));
    res.json(recordService.list({ type, category, from_date, to_date, page, limit }));
  } catch (err) {
    next(err);
  }
});

/**
 * GET /api/records/:id
 * Roles: viewer, analyst, admin
 */
router.get('/:id', (req, res, next) => {
  try {
    res.json(recordService.getById(Number(req.params.id)));
  } catch (err) {
    next(err);
  }
});

/**
 * POST /api/records
 * Roles: admin only
 */
router.post('/', authorize('admin'), createRules, validate, (req, res, next) => {
  try {
    const record = recordService.create(req.body, req.user.id);
    res.status(201).json(record);
  } catch (err) {
    next(err);
  }
});

/**
 * PUT /api/records/:id
 * Roles: admin only
 */
router.put('/:id', authorize('admin'), updateRules, validate, (req, res, next) => {
  try {
    res.json(recordService.update(Number(req.params.id), req.body));
  } catch (err) {
    next(err);
  }
});

/**
 * DELETE /api/records/:id  (soft delete)
 * Roles: admin only
 */
router.delete('/:id', authorize('admin'), (req, res, next) => {
  try {
    recordService.softDelete(Number(req.params.id));
    res.status(204).send();
  } catch (err) {
    next(err);
  }
});

module.exports = router;
