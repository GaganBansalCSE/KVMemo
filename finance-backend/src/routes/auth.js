'use strict';

const { Router } = require('express');
const { register: registerRules, login: loginRules } = require('../validators/auth');
const { validate } = require('../middleware/validate');
const authService = require('../services/authService');

const router = Router();

/**
 * POST /api/auth/register
 * Register a new user (default role: viewer).
 */
router.post('/register', registerRules, validate, (req, res, next) => {
  try {
    const result = authService.register(req.body);
    res.status(201).json(result);
  } catch (err) {
    next(err);
  }
});

/**
 * POST /api/auth/login
 * Authenticate and receive a JWT token.
 */
router.post('/login', loginRules, validate, (req, res, next) => {
  try {
    const result = authService.login(req.body);
    res.json(result);
  } catch (err) {
    next(err);
  }
});

module.exports = router;
