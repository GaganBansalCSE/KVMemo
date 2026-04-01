'use strict';

const jwt = require('jsonwebtoken');

const JWT_SECRET = process.env.JWT_SECRET || 'finance-dev-secret-change-in-prod';
const JWT_EXPIRES_IN = process.env.JWT_EXPIRES_IN || '8h';

/**
 * Middleware: verify JWT from Authorization header and attach decoded user to req.user.
 */
function authenticate(req, res, next) {
  const header = req.headers.authorization;
  if (!header || !header.startsWith('Bearer ')) {
    return res.status(401).json({ error: 'Missing or invalid Authorization header.' });
  }

  const token = header.slice(7);
  try {
    const payload = jwt.verify(token, JWT_SECRET);
    req.user = payload; // { id, username, role }
    next();
  } catch {
    return res.status(401).json({ error: 'Token is invalid or expired.' });
  }
}

/**
 * Factory: role-based access guard.
 * Usage: authorize('admin') or authorize('analyst', 'admin')
 */
function authorize(...allowedRoles) {
  return (req, res, next) => {
    if (!req.user) {
      return res.status(401).json({ error: 'Authentication required.' });
    }
    if (!allowedRoles.includes(req.user.role)) {
      return res.status(403).json({
        error: `Access denied. Required role(s): ${allowedRoles.join(', ')}.`,
      });
    }
    next();
  };
}

/**
 * Sign a JWT for the given user.
 */
function signToken(user) {
  return jwt.sign(
    { id: user.id, username: user.username, role: user.role },
    JWT_SECRET,
    { expiresIn: JWT_EXPIRES_IN }
  );
}

module.exports = { authenticate, authorize, signToken };
