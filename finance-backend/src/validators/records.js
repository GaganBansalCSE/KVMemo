'use strict';

const { body } = require('express-validator');

const VALID_TYPES      = ['income', 'expense'];
const MAX_CATEGORY_LEN = 50;
const MAX_NOTES_LEN    = 500;

const create = [
  body('amount')
    .isFloat({ gt: 0 }).withMessage('Amount must be a positive number.'),
  body('type')
    .isIn(VALID_TYPES).withMessage(`Type must be one of: ${VALID_TYPES.join(', ')}.`),
  body('category')
    .trim()
    .isLength({ min: 1, max: MAX_CATEGORY_LEN })
    .withMessage(`Category is required and must not exceed ${MAX_CATEGORY_LEN} characters.`),
  body('date')
    .isISO8601({ strict: true }).withMessage('Date must be a valid ISO 8601 date (YYYY-MM-DD).')
    .toDate(),
  body('notes')
    .optional({ nullable: true })
    .trim()
    .isLength({ max: MAX_NOTES_LEN })
    .withMessage(`Notes must not exceed ${MAX_NOTES_LEN} characters.`),
];

const update = [
  body('amount')
    .optional()
    .isFloat({ gt: 0 }).withMessage('Amount must be a positive number.'),
  body('type')
    .optional()
    .isIn(VALID_TYPES).withMessage(`Type must be one of: ${VALID_TYPES.join(', ')}.`),
  body('category')
    .optional()
    .trim()
    .isLength({ min: 1, max: MAX_CATEGORY_LEN })
    .withMessage(`Category must not exceed ${MAX_CATEGORY_LEN} characters.`),
  body('date')
    .optional()
    .isISO8601({ strict: true }).withMessage('Date must be a valid ISO 8601 date (YYYY-MM-DD).')
    .toDate(),
  body('notes')
    .optional({ nullable: true })
    .trim()
    .isLength({ max: MAX_NOTES_LEN })
    .withMessage(`Notes must not exceed ${MAX_NOTES_LEN} characters.`),
];

module.exports = { create, update };
