CREATE DATABASE IF NOT EXISTS mywebserver_test
  DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS mywebserver_test.user_basic_info (
  user_id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  user_name       VARCHAR(32)  NOT NULL,
  hashed_password VARBINARY(32) NOT NULL,
  salt            VARBINARY(16) NOT NULL,
  hash_algo       VARCHAR(32)  NOT NULL,
  iterations      INT UNSIGNED NOT NULL,
  hobby           VARCHAR(32)  NULL,
  sex             VARCHAR(8)   NULL,
  introduction    VARCHAR(1024) NULL,
  address         VARCHAR(64)  NULL,
  created_time    TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uk_user_name (user_name)
) ENGINE=InnoDB;
