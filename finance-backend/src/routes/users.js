'use strict';

const { Router } = require('express');
const { authenticate, authorize } = require('../middleware/auth');
const { validate } = require('../middleware/validate');
const { updateUser } = require('../validators/users');
const userService = require('../services/userService');

const router = Router();

// All user-management routes require authentication + admin role.
router.use(authenticate, authorize('admin'));

/**
 * GET /api/users?page=1&limit=20
 */
router.get('/', (req, res, next) => {
  try {
    const page  = Math.max(1, parseInt(req.query.page,  10) || 1);
    const limit = Math.min(100, Math.max(1, parseInt(req.query.limit, 10) || 20));
    res.json(userService.list({ page, limit }));
  } catch (err) {
    next(err);
  }
});

/**
 * GET /api/users/:id
 */
router.get('/:id', (req, res, next) => {
  try {
    res.json(userService.getById(Number(req.params.id)));
  } catch (err) {
    next(err);
  }
});

/**
 * PUT /api/users/:id
 * Update role, status, or email.
 */
router.put('/:id', updateUser, validate, (req, res, next) => {
  try {
    const id = Number(req.params.id);
    // Prevent admin from accidentally deactivating themselves
    if (id === req.user.id && req.body.status === 'inactive') {
      return res.status(400).json({ error: 'You cannot deactivate your own account.' });
    }
    res.json(userService.update(id, req.body));
  } catch (err) {
    next(err);
  }
});

/**
 * DELETE /api/users/:id
 */
router.delete('/:id', (req, res, next) => {
  try {
    const id = Number(req.params.id);
    if (id === req.user.id) {
      return res.status(400).json({ error: 'You cannot delete your own account.' });
    }
    userService.remove(id);
    res.status(204).send();
  } catch (err) {
    next(err);
  }
});

module.exports = router;
