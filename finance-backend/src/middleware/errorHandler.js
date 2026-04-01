'use strict';

/**
 * Global error-handling middleware.
 * Must be registered last (after all routes).
 */
function errorHandler(err, req, res, next) { // eslint-disable-line no-unused-vars
  if (process.env.NODE_ENV !== 'test') {
    console.error(err);
  }

  // express-validator passes errors via next(err) with a specific shape; guard against it
  if (res.headersSent) return next(err);

  const status = err.status || err.statusCode || 500;
  const message = status < 500 ? err.message : 'Internal server error.';

  res.status(status).json({ error: message });
}

module.exports = { errorHandler };
