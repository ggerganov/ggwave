import type { FormEvent } from 'react';
import { useEffect, useMemo, useState } from 'react';
import type { AdminUser } from '../api/aat';
import { createUser, listUsers } from '../api/aat';

const emptyForm = {
  userId: '',
  displayName: '',
};

export function AdminPanel() {
  const [users, setUsers] = useState<AdminUser[]>([]);
  const [form, setForm] = useState(emptyForm);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [copyMessage, setCopyMessage] = useState<string | null>(null);

  const sortedUsers = useMemo(
    () => [...users].sort((a, b) => a.userId.localeCompare(b.userId)),
    [users],
  );

  const fetchUsers = async () => {
    try {
      setError(null);
      const response = await listUsers();
      setUsers(response.users);
    } catch (err) {
      setError('Не удалось загрузить список пользователей.');
    }
  };

  useEffect(() => {
    fetchUsers();
  }, []);

  const handleSubmit = async (event: FormEvent) => {
    event.preventDefault();
    if (loading) {
      return;
    }
    if (!form.userId.trim()) {
      setError('Введите идентификатор пользователя.');
      return;
    }
    setLoading(true);
    setError(null);
    try {
      const created = await createUser({
        userId: form.userId.trim(),
        displayName: form.displayName.trim() || undefined,
      });
      setUsers((prev) => [...prev, created]);
      setForm(emptyForm);
    } catch (err) {
      setError(
        err instanceof Error ? err.message : 'Ошибка при создании пользователя. Повторите позже.',
      );
    } finally {
      setLoading(false);
    }
  };

  const handleCopy = async (code: string) => {
    try {
      await navigator.clipboard.writeText(code);
      setCopyMessage('Код скопирован. Вставьте его в мобильное приложение.');
      setTimeout(() => setCopyMessage(null), 3000);
    } catch (err) {
      setError('Не удалось скопировать код. Скопируйте вручную.');
    }
  };

  return (
    <div className="admin">
      <section className="admin__card">
        <h2>Управление пользователями</h2>
        <p>
          Создайте профиль пользователя, затем передайте ему код инициализации. Этот код нужно один
          раз вставить в мобильное приложение, чтобы устройство получило seed и идентификаторы.
        </p>

        <form className="admin__form" onSubmit={handleSubmit}>
          <label className="admin__label">
            Идентификатор пользователя (userId)
            <input
              type="text"
              value={form.userId}
              onChange={(event) => setForm((prev) => ({ ...prev, userId: event.target.value }))}
              placeholder="например, operator-1"
              required
            />
          </label>
          <label className="admin__label">
            Отображаемое имя (необязательно)
            <input
              type="text"
              value={form.displayName}
              onChange={(event) => setForm((prev) => ({ ...prev, displayName: event.target.value }))}
              placeholder="Имя сотрудника"
            />
          </label>
          <button className="app__button" type="submit" disabled={loading}>
            {loading ? 'Создание...' : 'Создать пользователя'}
          </button>
        </form>

        {error && <div className="app__error">{error}</div>}
        {copyMessage && <div className="admin__notice">{copyMessage}</div>}

        <div className="admin__table-wrapper">
          <table className="admin__table">
            <thead>
              <tr>
                <th>userId</th>
                <th>Имя</th>
                <th>kid</th>
                <th>scope</th>
                <th>Provisioning код</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {sortedUsers.map((user) => (
                <tr key={user.kid}>
                  <td>{user.userId}</td>
                  <td>{user.displayName ?? '—'}</td>
                  <td>{user.kid}</td>
                  <td>
                    type {user.scopeType}, id {user.scopeId}
                  </td>
                  <td className="admin__code">
                    <code>{user.provisioningCode}</code>
                  </td>
                  <td>
                    <button
                      type="button"
                      className="app__button app__secondary"
                      onClick={() => handleCopy(user.provisioningCode)}
                    >
                      Скопировать
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <aside className="admin__help">
          <h3>Как использовать код на телефоне</h3>
          <ol>
            <li>Откройте приложение EchoPass на Android.</li>
            <li>Нажмите «Импортировать профиль» и вставьте скопированный код.</li>
            <li>После сохранения профиль станет активным, можно воспроизводить токены.</li>
          </ol>
        </aside>
      </section>
    </div>
  );
}

