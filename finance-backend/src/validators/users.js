'use strict';

const { body } = require('express-validator');

const VALID_ROLES    = ['viewer', 'analyst', 'admin'];
const VALID_STATUSES = ['active', 'inactive'];

const updateUser = [
  body('role')
    .optional()
    .isIn(VALID_ROLES)
    .withMessage(`Role must be one of: ${VALID_ROLES.join(', ')}.`),
  body('status')
    .optional()
    .isIn(VALID_STATUSES)
    .withMessage(`Status must be one of: ${VALID_STATUSES.join(', ')}.`),
  body('email')
    .optional()
    .trim()
    .isEmail().withMessage('Must be a valid email address.')
    .normalizeEmail(),
];

module.exports = { updateUser };
