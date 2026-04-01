# Finance Backend

A REST API backend for a **Finance Dashboard** system built with **Node.js**, **Express**, and **SQLite** (`better-sqlite3`).

It implements:
- User management with JWT-based authentication
- Role-based access control (Viewer / Analyst / Admin)
- Financial records CRUD with filtering and soft-delete
- Dashboard summary, category breakdown, monthly trends, and recent-activity endpoints
- Input validation and structured error responses

---

## Tech Stack

| Concern        | Library                    |
|----------------|----------------------------|
| HTTP framework | Express 4                  |
| Database       | SQLite via `better-sqlite3`|
| Auth           | JWT (`jsonwebtoken`)       |
| Passwords      | `bcryptjs`                 |
| Validation     | `express-validator`        |
| Testing        | Node.js built-in `node:test` |

---

## Project Structure

```
finance-backend/
├── src/
│   ├── app.js                  # Express app setup and route registration
│   ├── db/
│   │   ├── database.js         # SQLite connection + schema initialisation
│   │   └── seed.js             # Demo data seeder
│   ├── middleware/
│   │   ├── auth.js             # JWT authenticate / authorize middleware
│   │   ├── errorHandler.js     # Global error handler
│   │   └── validate.js         # express-validator result checker
│   ├── routes/
│   │   ├── auth.js             # POST /api/auth/register|login
│   │   ├── users.js            # GET|PUT|DELETE /api/users (Admin)
│   │   ├── records.js          # CRUD /api/records
│   │   └── dashboard.js        # GET /api/dashboard/*
│   ├── services/
│   │   ├── authService.js      # Registration and login logic
│   │   ├── userService.js      # User CRUD logic
│   │   ├── recordService.js    # Financial record CRUD + filtering
│   │   └── dashboardService.js # Aggregation queries
│   └── validators/
│       ├── auth.js             # Register / login rules
│       ├── records.js          # Record create / update rules
│       └── users.js            # User update rules
├── tests/
│   └── api.test.js             # Integration tests (21 cases)
├── .env.example
├── .gitignore
└── package.json
```

---

## Setup

### Prerequisites

- Node.js ≥ 18
- A C++ compiler (gcc/g++) is needed to build the native SQLite binding on first install

### 1. Install dependencies

```bash
cd finance-backend
npm install
```

### 2. Configure environment

```bash
cp .env.example .env
# Edit .env — at minimum, change JWT_SECRET for any real deployment
```

### 3. Seed demo data (optional)

```bash
npm run seed
```

This creates three ready-to-use accounts and 15 sample records:

| Role    | Username | Password     |
|---------|----------|--------------|
| admin   | admin    | Admin@123    |
| analyst | analyst  | Analyst@123  |
| viewer  | viewer   | Viewer@123   |

### 4. Start the server

```bash
npm start          # production
npm run dev        # development (auto-reload via nodemon)
```

The server starts on **http://localhost:3000** (or `PORT` from `.env`).

---

## Running Tests

```bash
npm test
```

The test suite uses an in-memory SQLite database so it never touches the production database file. All 21 integration tests cover auth, records, dashboard, and user management.

---

## API Reference

All protected routes require the header:
```
Authorization: Bearer <token>
```

Tokens are obtained from the login or register endpoints.

### Health

| Method | Path      | Auth | Description      |
|--------|-----------|------|------------------|
| GET    | `/health` | –    | Liveness check   |

---

### Authentication

#### `POST /api/auth/register`
Register a new user. New accounts default to the `viewer` role.

**Request body:**
```json
{
  "username": "alice",
  "email": "alice@example.com",
  "password": "SecurePass1"
}
```

**Response `201`:**
```json
{
  "token": "<jwt>",
  "user": { "id": 1, "username": "alice", "email": "alice@example.com", "role": "viewer", "status": "active", "created_at": "..." }
}
```

---

#### `POST /api/auth/login`
Authenticate and receive a JWT.

**Request body:**
```json
{ "username": "admin", "password": "Admin@123" }
```

**Response `200`:**
```json
{ "token": "<jwt>", "user": { ... } }
```

---

### Users *(Admin only)*

| Method | Path             | Description                          |
|--------|------------------|--------------------------------------|
| GET    | `/api/users`     | List all users (pagination: `page`, `limit`) |
| GET    | `/api/users/:id` | Get a user by ID                     |
| PUT    | `/api/users/:id` | Update `role`, `status`, or `email`  |
| DELETE | `/api/users/:id` | Permanently delete a user            |

**PUT body** (all fields optional):
```json
{ "role": "analyst", "status": "inactive", "email": "new@example.com" }
```

---

### Financial Records

| Method | Path               | Roles               | Description                     |
|--------|--------------------|---------------------|---------------------------------|
| GET    | `/api/records`     | viewer, analyst, admin | List records with filtering  |
| GET    | `/api/records/:id` | viewer, analyst, admin | Get a single record          |
| POST   | `/api/records`     | admin               | Create a record                 |
| PUT    | `/api/records/:id` | admin               | Update a record                 |
| DELETE | `/api/records/:id` | admin               | Soft-delete a record            |

#### Filtering (GET `/api/records`)

| Query param | Example            | Description                |
|-------------|--------------------|----------------------------|
| `type`      | `income`           | Filter by income / expense |
| `category`  | `Salary`           | Case-insensitive match     |
| `from_date` | `2024-01-01`       | Start date (inclusive)     |
| `to_date`   | `2024-03-31`       | End date (inclusive)       |
| `page`      | `2`                | Page number (default 1)    |
| `limit`     | `10`               | Page size (default 20, max 100) |

**POST / PUT body:**
```json
{
  "amount": 1500.00,
  "type": "income",
  "category": "Salary",
  "date": "2024-05-15",
  "notes": "May salary"
}
```

---

### Dashboard

| Method | Path                              | Roles           | Description                            |
|--------|-----------------------------------|-----------------|----------------------------------------|
| GET    | `/api/dashboard/summary`          | analyst, admin  | Total income, expenses, net balance    |
| GET    | `/api/dashboard/category-breakdown` | analyst, admin | Category-wise totals and stats         |
| GET    | `/api/dashboard/trends`           | analyst, admin  | Monthly income / expense totals        |
| GET    | `/api/dashboard/recent`           | all             | N most recent records (default 10)     |

All dashboard endpoints accept optional `from_date` and `to_date` query parameters (ISO 8601).

#### `GET /api/dashboard/summary` response
```json
{
  "total_income": 21250.00,
  "total_expenses": 6373.50,
  "net_balance": 14876.50,
  "record_count": 15
}
```

#### `GET /api/dashboard/category-breakdown` response
```json
[
  { "category": "Salary", "type": "income", "record_count": 4, "total": 20500.00, "average": 5125.00, "min": 5000.00, "max": 5500.00 },
  ...
]
```

#### `GET /api/dashboard/trends` response
```json
[
  { "month": "2024-01", "type": "income",  "record_count": 1, "total": 5000.00 },
  { "month": "2024-01", "type": "expense", "record_count": 2, "total": 1550.50 },
  ...
]
```

---

## Role Summary

| Capability                         | Viewer | Analyst | Admin |
|------------------------------------|:------:|:-------:|:-----:|
| Register / Login                   | ✅     | ✅      | ✅    |
| View records                       | ✅     | ✅      | ✅    |
| Create / update / delete records   | ❌     | ❌      | ✅    |
| Dashboard summary & trends         | ❌     | ✅      | ✅    |
| Category breakdown                 | ❌     | ✅      | ✅    |
| View recent activity               | ✅     | ✅      | ✅    |
| Manage users                       | ❌     | ❌      | ✅    |

---

## Error Handling

| Status | Meaning                            |
|--------|------------------------------------|
| 400    | Bad request (e.g., no fields to update) |
| 401    | Missing or invalid JWT             |
| 403    | Authenticated but insufficient role |
| 404    | Resource not found                 |
| 409    | Conflict (duplicate username/email)|
| 422    | Validation failed — body includes `errors` array |
| 500    | Unexpected server error            |

Validation errors return an `errors` array:
```json
{
  "errors": [
    { "type": "field", "msg": "Amount must be a positive number.", "path": "amount", "location": "body" }
  ]
}
```

---

## Assumptions & Design Notes

1. **New registrations default to `viewer`.** An admin must manually promote a user to `analyst` or `admin`.
2. **Soft delete for records.** Deleted records set `deleted_at` and are excluded from all queries. This preserves historical data integrity.
3. **Hard delete for users.** Removing a user is a permanent operation (guarded so admins cannot delete themselves).
4. **Password policy**: minimum 8 characters, at least one uppercase letter and one digit.
5. **JWT lifetime**: 8 hours by default, configurable via `JWT_EXPIRES_IN`.
6. **SQLite WAL mode** is enabled for better concurrent read performance.
7. **No pagination on dashboard aggregates** — they are always full-scan queries filtered by date range.
8. **Date format**: ISO 8601 (`YYYY-MM-DD`) is required for all record dates and filter parameters.
