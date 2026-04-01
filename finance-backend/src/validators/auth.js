'use strict';

const { body } = require('express-validator');

const register = [
  body('username')
    .trim()
    .isLength({ min: 3, max: 30 }).withMessage('Username must be 3–30 characters.')
    .matches(/^[a-zA-Z0-9_]+$/).withMessage('Username may only contain letters, numbers, and underscores.'),
  body('email')
    .trim()
    .isEmail().withMessage('Must be a valid email address.')
    .normalizeEmail(),
  body('password')
    .isLength({ min: 8 }).withMessage('Password must be at least 8 characters.')
    .matches(/[A-Z]/).withMessage('Password must contain at least one uppercase letter.')
    .matches(/[0-9]/).withMessage('Password must contain at least one number.'),
];

const login = [
  body('username').trim().notEmpty().withMessage('Username is required.'),
  body('password').notEmpty().withMessage('Password is required.'),
];

module.exports = { register, login };
